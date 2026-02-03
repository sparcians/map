# TreeNode Extensions

## Overview

This feature introduces **TreeNode Extensions**, a mechanism for attaching opaque, user-defined objects to any `sparta::TreeNode` in the device tree.

Extensions allow users and framework components to associate additional behavior, metadata, or state with existing `TreeNode` instances **without modifying simulator topology** and **without participating in the normal tree construction lifecycle**.

Key properties:

- Extensions can be attached to **any** `TreeNode`
- Extensions are **opaque to the framework**
- Extensions may be **added and accessed at any time**
  - Before or after `buildTree()`
  - Before or after `finalizeTree()`
  - During simulation
  - After simulation
- Extensions are instantiated via **user-registered factories**
- Multiple extension types may coexist on the same `TreeNode`
- Only one **instance** of an extension type may exist on a `TreeNode`
- All extensions on a `TreeNode` are accessed by a unique name provided by extension subclasses

Visualization of a device with extensions:

```
top
  |-- core0
  |---- extensions
  |------ foo (core0 has-a FooExtension named "foo")
  |-------- params
  |---------- fiz: 4
  |---------- fuz: "hello"
  |---- fetch
  |---- decode
   ...
  |---- rob
  |------ extensions
  |-------- foo (ROB has-a FooExtension named "foo")
  |---------- params
  |------------ fiz: 6
  |------------ fuz: "world"
  |-------- bar (ROB has-a BarExtension named "bar")
  |---------- params
  |------------ biz: 8.9
  |------------ buz: [1,2,3,4,5]
```

However, if you call RootTreeNode::renderSubtree() (--show-tree) you
will NOT see the "extensions" nodes or their descendants as they are
not actually `TreeNodes`.

---

## Motivation

Sparta’s device tree enforces strict rules around `TreeNode` creation:

- Structural changes are forbidden after `finalizeTree()`
- Tree integrity is tightly controlled for correctness

While this is essential for simulation correctness, it makes it difficult to:

- Attach new functionality to existing nodes
- Experiment with new features without modifying core classes
- Share metadata between tools and simulation components
- Add late-bound or optional behaviors

**TreeNode Extensions** solve this by providing a controlled, non-invasive attachment mechanism that does **not** alter the tree structure or lifecycle.

---

## Design Goals

This feature is explicitly designed to:

- Avoid modifying existing `TreeNode` subclasses
- Preserve Sparta’s strict tree lifecycle rules
- Enable experimentation without framework changes
- Allow tools and simulators to layer functionality cleanly
- Keep extensions opaque and decoupled from core framework logic

---

## Non-Goals

Extensions are **not** intended to:

- Replace `TreeNode` subclasses
- Add new tree structure
- Participate in scheduling or event ordering
- Modify core simulation semantics
- Circumvent tree integrity rules

---

## Conceptual Model

A TreeNode extension is:

- A user-defined C++ object
- Associated with exactly one `TreeNode`
- Created on demand if a factory exists (can be `dynamic_cast` to `YourExtensionSubclass*`)
  - `TreeNode::getExtension(name)`
  - `TreeNode::getExtensionAs<YourExtensionSubclass>(name)`
  - `TreeNode::getExtension() // must only have one extension`
    - NOTE: Only the **non-const** versions of these APIs will create the extension
    - NOTE: If these methods are called without a factory, and no such extension exists, one will NOT be created on demand
  - `TreeNode::createExtension(name)`
- Created using string-only parameters if no factory exists
  - `TreeNode::createExtension(name)`
  - Returns an `ExtensionsParamsOnly*` which cannot be `dynamic_cast` to anything

Think of extensions as a **type-indexed side table** attached to each `TreeNode`.

---

## Extension Lifecycle

Extensions have a **completely independent lifecycle** from the Sparta tree.

### What extensions are NOT subject to

Extensions:

- Are **not** `TreeNodes`
- Do **not** affect tree topology
- Do **not** violate tree immutability rules
- Do **not** care about tree phases (building, configuring, etc.)

### When extensions can be used

Extensions may be:

- Registered anytime before `configureTree()`
- Added to `TreeNodes` at any time
- Accessed at any time
- Lazily instantiated on first access (**requires factory**)

This makes extensions ideal for:

- Late-bound instrumentation
- Hidden metadata / implementation detail
- Optional features controlled by configuration

---

## Extension Factories

Extensions are typically created via **registered factories**.

- You may add this macro to any `.cpp` file built by your simulator:
  - REGISTER_TREE_NODE_EXTENSION(YourExtensionSubclass)
  - Provides earliest possible factory registration
- You can also call `TreeNode::addExtensionFactory()`
  - Can only call before `configureTree()`
- You can also call `app::Simulation::addTreeNodeExtensionFactory_()`
  - Can only call before `configureTree()`

### No Extension Factory?

If you want to use extensions with simple parameters, where your extension does not provide any additional behavior/methods, you do not need a factory.

Rules:
- You can no longer depend on implicit / lazy extension creation using `TreeNode::getExtension(name)`
- You must call `TreeNode::createExtension(name[, replace=false])`
- For `replace=false`:
  - If an extension already exists for this node by this name, return it
  - Otherwise, create a new one and return it
- For `replace=true`:
  - Create a new extension and return it

For extensions with string-only parameters, extensions are always of type `ExtensionsParamsOnly`.

- If you want to parse the parameter strings to get specific data types:
  - Call `ExtensionsBase::getParameterValueAs<T>(name)`
  - `<T>` types supported include signed and unsigned 8/16/32/64-bit integers, doubles, strings, and bools
    - `getParameterValueAs<bool>(name)`
  - `<T>` types supported also include 1-D or 2-D vectors of the above types
    - `getParameterValueAs<std::vector<bool>>(name)`
    - `getParameterValueAs<std::vector<std::vector<bool>>>(name)`
  - All other `<T>` types will fail to compile with a `static_assert`

---

## Examples

### Example: Defining an Extension

```
#include "sparta/simulation/TreeNodeExtensions.hpp"

class CoreExtensions : public sparta::ExtensionsParamsOnly
{
public:
    // NOTE: Every extension subclass needs a constexpr NAME. This is used in
    // the extension/arch/config files for parameterization. See the section
    // "Parameterizing an Extension".
    static constexpr auto NAME = "core_extensions";

    // Optionally override to add more parameters and set default values
    void postCreate() override {
        auto param_set = getParameters();
        extra_param_.reset(new sparta::Parameter<double>(
            "extra_param", 3.14, "An extra parameter", param_set));
    }

private:
    std::unique_ptr<sparta::Parameter<double>> extra_param_;
};
```

---

### Example: Registering an Extension

Extension registration can be done several ways:

- Registration macro (recommended)
- Using `TreeNode` public API
- Using `app::Simulation` protected API

If you want your extension to be registered as soon as possible, call the macro from a translation unit:

```
// CoreExtensions.cpp

#include "sparta/simulation/TreeNodeExtensions.hpp"
class CoreExtensions : public sparta::ExtensionsParamsOnly { ... };

#include "sparta/simulation/RootTreeNode.hpp"
REGISTER_TREE_NODE_EXTENSION(CoreExtensions);
```

The `TreeNode` and `app::Simulation` APIs can also be used, but remember to make the calls prior to `configureTree()`.

```
void MySimulator::buildTree_()
{
    // Using TreeNode API
    auto rtn = getRoot();
    rtn->addExtensionFactory(CoreExtensions::NAME,
        [](){ return new CoreExtensions; });

    // Using Simulation API
    //   addTreeNodeExtensionFactory_(CoreExtensions::NAME,
    //       [](){ return new CoreExtensions; });

    // Proceed with buildTree_() details
    ...
}
```

---

### Example: Parameterizing an Extension

Parameterize your extensions either by inlining their parameters in an arch/config file:

```
# small_cores.yaml
top.cpu.core*:
  fetch.params:
    num_to_fetch: 4                 # PARAMETER() added in Fetch unit. Nothing to do with extensions.
  extension:
    core_extensions:                # constexpr NAME in your extension subclass
      enabled_features:  [foo,bar]  # Set YAML-only parameter (not added in postCreate)
      extra_param: 5.67             # Overridden parameter value (added in postCreate with default value)

# Run simulator
./sim --arch small_cores.yaml --arch-search-dir .
./sim --config small_cores.yaml --config-search-dir .
```

Or you can strip out the extensions and put them by themselves in a separate YAML file:

```
# extensions.yaml
top.cpu.core*.extension:
  core_extensions:                # constexpr NAME in your extension subclass
    enabled_features:  [foo,bar]  # Set YAML-only parameter (not added in postCreate)
    extra_param: 5.67             # Overridden parameter value (added in postCreate with default value)

# Run simulator
./sim --extension-file extensions.yaml
```

You can also inline the extensions right on the command line:

```
./sim -p top.cpu.core*.extension.core_extensions.enabled_features '[foo,bar]'
      -p top.cpu.core*.extension.core_extensions.extra_param 5.67
```

NOTE: If you use `-p` at the command line, then these extensions/parameters MUST be read by the time `finalizeTree()` is called, else you get an exception.

---

### Example: Accessing an Extension

There are several ways to access an extension:

---

#### TreeNode::createExtension(name, replace=false)

If the extension already exists and `replace=false`, return it, otherwise:
- If a factory exists, invoke it and return the extension
- If a factory does not exist, create and return an `ExtensionsParamsOnly`

These are equivalent:

`auto ext = tn->createExtension(name, true /*replace*/);`

and

```
tn->removeExtension(name);
auto ext = tn->createExtension(name, false /*don't replace*/);
```

---

#### TreeNode::getExtension(name)

- If the extension exists on this node, return it, otherwise:
  - If a factory exists, invoke it and return the extension
  - If no factory exists, return nullptr, **not** `ExtensionsParamsOnly`

NOTE: If the tree node's path is not in any extension/arch/config file, this **never** creates an extension, even with a registered factory.

---

#### TreeNode::getExtension(name) const

The `const` version of `getExtension(name)` will only return the extension if it already exists.

---

#### TreeNode::getExtension()

- If zero extensions exist, return nullptr
- If **exactly** one extension exists, same as the **non-const** `getExtension(only_extension_name)`
- If more than one extension exists, throws an exception

---

#### TreeNode::getExtension() const

- If zero extensions exist, return nullptr
- If **exactly** one extension exists, same as the **const** `getExtension(only_extension_name)`
- If more than one extension exists, throws an exception

---

#### TreeNode::getExtensionAs\<T\>(name)

- Same basic rules as the **non-const** `getExtension(name)` API
- Throws an exception if an extension by this name exists, but the downcast fails

---

#### TreeNode::getExtensionAs\<T\>(name) const

- Same basic rules as the **const** `getExtension(name)` API
- Throws an exception if an extension by this name exists, but the downcast fails

---

#### Example: Removing an Extension

```
if (tn->hasExtension("core_extensions")) {
    sparta_assert(tn->removeExtension("core_extensions"));
    sparta_assert(!tn->hasExtension("core_extensions"));
}

// You do not need to check hasExtension() first; removeExtension() returns
// a flag saying whether it was removed (true) or did not exist (false).
```

---

## What about `--write-final-config`?

What **does** get added to the final config YAML file?
- All extensions created in `buildTree()`, `configureTree()`, and `finalizeTree()`
- Default parameter values specified in `postCreate()`
- NOTE: Removing extensions after `finalizeTree()` does not omit them from the final YAML file

What **does not** get added to the final config YAML file?
- Any extensions created in `finalizeFramework()`
- Any extensions created during or after simulation
- Extension parameter changes that occur after `finalizeTree()`

---

## Summary

TreeNode Extensions provide a powerful and flexible way to associate user-defined behavior and state with Sparta’s device tree without compromising the framework’s design principles.
