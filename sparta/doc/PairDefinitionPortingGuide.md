# PairDefinition Porting Guide for Legacy Sparta Collectables

This document is for an AI assistant helping port existing Sparta simulator code from legacy `operator<<`
collection to `sparta::PairDefinition`. It is not user-facing documentation.

The goal is to translate struct-like collected types into field-by-field PairDefinitions with as
little guesswork as possible, while still asking for confirmation when the mapping is ambiguous.

## What PairDefinition Is For

PairDefinition is used to collect struct-like data by registering named fields and getters.
It replaces the old ostream-based collection path, which is slower and produces larger datasets.

Use PairDefinition when collecting a type that is not a simple POD, enum, or string and is being
collected as structured data.

## Where PairDefinition Is Required

First normalize the collected type `T`:

- If `T` is a raw pointer, smart pointer, or similar pointer-like wrapper, use the effective pointee
  type. Sparta already strips these kinds of wrappers in its collection traits.
- If the effective `T` is a POD, enum, or string, PairDefinition is not needed.
- If using `sparta::collection::Collectable<T>`, `DelayedCollectable<T>`, or
  `IterableCollector<T>` and `T` is struct-like, PairDefinition is required. This are explicitly
  collected.
- If using an implicitly collected type such as `sparta::Array`, `sparta::Queue`, `sparta::Pipe`,
  `sparta::Pipeline`, `sparta::PriorityQueue`, `sparta::Buffer`, `sparta::CircularBuffer`,
  `sparta::FrontArray`, `sparta::AgedArrayCollector`, `sparta::DataInPort`, `sparta::SyncInPort`,
  `sparta::SyncOutPort`, or `sparta::Bus`, then PairDefinition is required only if collection is
  actually enabled for that instance. See enableCollection() notes below.

### Implicitly collected types

- If the code calls `enableCollection()`, then a struct-like `T` must have PairDefinition.
- If the code never calls `enableCollection()`, ask whether the user intends to enable collection
  in the future.
- If the answer is yes, PairDefinition can be added now (not required, but good to know for the future at least).
- If the answer is no, PairDefinition is not needed now.

### What happens if PairDefinition is missing later

- For non-port implicitly collected types such as `Array`, `Queue`, `Pipe`, `Pipeline`, `Buffer`,
  `CircularBuffer`, `FrontArray`, and similar, a later `enableCollection()` will static_assert.
- For `DataInPort`, `SyncInPort`, and `SyncOutPort`, a later `enableCollection()` will static_assert.
- For `Bus`, a later `enableCollection()` will throw instead of static_assert because `Bus` holds
  `Port` base-class pointers and invokes collection through virtual methods.

## How To Translate A Legacy operator<<

Use the legacy ostream operator as the first hint for which fields are worth collecting.
The translation is usually mechanical.

### Basic recipe

1. Keep the existing type.
2. Forward declare the PairDefinition class. If the user's class is MyFoo, use the convention MyFooPairDef.
3. Add `using SpartaPairDefinitionType = <ThatPairDefinitionClass>;` to the collected type. (e.g. MyFooPairDef).
4. Define the PairDefinition (MyFooPairDef) directly below the user's class (MyFoo).
5. Include `sparta/pairs/SpartaKeyPairs.hpp` wherever the PairDefinition is defined.
6. Register each collected field using `SPARTA_ADDPAIR` or `SPARTA_FLATTEN`.
7. Preserve the surrounding file formatting and comment style.

## Standalone Setup Assumptions

This guide must work even when a simulator has no existing PairDefinitions and Sparta was built and
installed separately.

- Always include `sparta/pairs/SpartaKeyPairs.hpp` in the file where PairDefinitions are declared.
- Treat `sparta/pairs/SpartaKeyPairs.hpp` as the required include contract for PairDefinition use.
- Assume users compile against installed Sparta include paths (for example system, local install, or
  conda environment), where `sparta/pairs/SpartaKeyPairs.hpp` resolves through their build system.
- If include resolution is unclear, ask the user where Sparta include directories are configured
  instead of hardcoding environment-specific paths.

### Standard shape

```cpp
class MyTypePairDef;

class MyType
{
public:
    using SpartaPairDefinitionType = MyTypePairDef;
    // getters...
};

class MyTypePairDef : public sparta::PairDefinition<MyType>
{
public:
    MyTypePairDef() : PairDefinition<MyType>()
    {
        SPARTA_INVOKE_PAIRS(MyType);
    }

    SPARTA_REGISTER_PAIRS(
        SPARTA_ADDPAIR("field", &MyType::getField),
        SPARTA_ADDPAIR("addr", &MyType::getAddress, std::ios::hex))
};
```

Field names e.g. "field" and "addr" must be unique.

## Choosing Between ADDPAIR and FLATTEN

Use `SPARTA_ADDPAIR` for a field that should appear as a named value in the collection output.

Use `SPARTA_FLATTEN` when a getter returns another collectable, struct-like object whose fields
should be expanded into the current PairDefinition instead of kept as a nested object. In order
to use SPARTA_FLATTEN, the referenced struct-like object must provide its own PairDefinition,
else the code will not compile.

You can mix ADDPAIR and FLATTEN freely. Order does not matter.

FLATTEN does not take a field name. ADDPAIR always requires a field name.

### Examples

## End-To-End Translation Examples (No Existing PairDefinitions)

This section is intentionally complete. It shows fully defined dummy classes, legacy ostream
operators, translated PairDefinitions, and collection call sites.

### Example 1: Direct collectable with pointer type

Input code (legacy):

```cpp
#include <cstdint>
#include <memory>
#include <ostream>
#include <string>

class Inst
{
private:
  int32_t uid_ = 0;
  uint64_t opcode_ = 0;
  std::string mnemonic_;

public:
  int32_t getUID() const { return uid_; }
  uint64_t getOpcode() const { return opcode_; }
  const std::string& getMnemonic() const { return mnemonic_; }
};

inline std::ostream& operator<<(std::ostream& os, const Inst& inst)
{
  os << "uid(" << inst.getUID() << ") ";
  os << "opc(" << std::hex << inst.getOpcode() << ") ";
  return os << "mnemonic(" << inst.getMnemonic() << ")";
}

using InstPtr = std::shared_ptr<Inst>;

// Pointer-like wrapper means effective collected type is Inst.
sparta::collection::Collectable<InstPtr> inst_collectable;
```

Translated code:

```cpp
#include "sparta/pairs/SpartaKeyPairs.hpp"

class InstPairDef;

class Inst
{
private:
  int32_t uid_ = 0;
  uint64_t opcode_ = 0;
  std::string mnemonic_;

public:
  using SpartaPairDefinitionType = InstPairDef;

  int32_t getUID() const { return uid_; }
  uint64_t getOpcode() const { return opcode_; }
  const std::string& getMnemonic() const { return mnemonic_; }
};

class InstPairDef : public sparta::PairDefinition<Inst>
{
public:
  InstPairDef() : PairDefinition<Inst>()
  {
    SPARTA_INVOKE_PAIRS(Inst);
  }

  SPARTA_REGISTER_PAIRS(
    SPARTA_ADDPAIR("uid", &Inst::getUID),
    SPARTA_ADDPAIR("opc", &Inst::getOpcode, std::ios::hex),
    SPARTA_ADDPAIR("mnemonic", &Inst::getMnemonic))
};
```

### Example 2: Nested object forwarded in operator<< maps to FLATTEN

Input code (legacy):

```cpp
class OuterWithCollectedInst
{
private:
  InstPtr inst_;

public:
  InstPtr getInst() const { return inst_; }
};

inline std::ostream& operator<<(std::ostream& os, const OuterWithCollectedInst& outer)
{
  return os << *outer.getInst();
}

sparta::collection::Collectable<OuterWithCollectedInst> outer_collectable;
```

Translated code:

```cpp
#include "sparta/pairs/SpartaKeyPairs.hpp"

class OuterWithCollectedInstPairDef;

class OuterWithCollectedInst
{
private:
  InstPtr inst_;

public:
  using SpartaPairDefinitionType = OuterWithCollectedInstPairDef;

  InstPtr getInst() const { return inst_; }
};

class OuterWithCollectedInstPairDef : public sparta::PairDefinition<OuterWithCollectedInst>
{
public:
  OuterWithCollectedInstPairDef() : PairDefinition<OuterWithCollectedInst>()
  {
    SPARTA_INVOKE_PAIRS(OuterWithCollectedInst);
  }

  SPARTA_REGISTER_PAIRS(
    SPARTA_FLATTEN(&OuterWithCollectedInst::getInst))
};
```

### Example 3: Mixed FLATTEN and ADDPAIR

Input code (legacy):

```cpp
class IntAndInst
{
private:
  InstPtr inst_;
  int32_t value_ = 0;

public:
  InstPtr getInst() const { return inst_; }
  int32_t getValue() const { return value_; }
};

inline std::ostream& operator<<(std::ostream& os, const IntAndInst& both)
{
  return os << *both.getInst() << " intval(" << both.getValue() << ")";
}
```

Translated code:

```cpp
#include "sparta/pairs/SpartaKeyPairs.hpp"

class IntAndInstPairDef;

class IntAndInst
{
private:
  InstPtr inst_;
  int32_t value_ = 0;

public:
  using SpartaPairDefinitionType = IntAndInstPairDef;

  InstPtr getInst() const { return inst_; }
  int32_t getValue() const { return value_; }
};

class IntAndInstPairDef : public sparta::PairDefinition<IntAndInst>
{
public:
  IntAndInstPairDef() : PairDefinition<IntAndInst>()
  {
    SPARTA_INVOKE_PAIRS(IntAndInst);
  }

  SPARTA_REGISTER_PAIRS(
    SPARTA_FLATTEN(&IntAndInst::getInst),
    SPARTA_ADDPAIR("intval", &IntAndInst::getValue))
};
```

### Example 4: Implicitly collected types and enableCollection()

```cpp
class QueuePayloadPairDef;
class QueuePayload
{
public:
  using SpartaPairDefinitionType = QueuePayloadPairDef;
  uint64_t getVAddr() const;
  uint32_t getSize() const;
};

class QueuePayloadPairDef : public sparta::PairDefinition<QueuePayload>
{
public:
  QueuePayloadPairDef() : PairDefinition<QueuePayload>()
  {
    SPARTA_INVOKE_PAIRS(QueuePayload);
  }

  SPARTA_REGISTER_PAIRS(
    SPARTA_ADDPAIR("vaddr", &QueuePayload::getVAddr, std::ios::hex),
    SPARTA_ADDPAIR("size", &QueuePayload::getSize))
};

class ExampleUnit
{
private:
  sparta::Queue<QueuePayload> q_;

public:
  void setupCollection(sparta::TreeNode* node)
  {
    // Because this enableCollection call exists, QueuePayload needs PairDefinition.
    q_.enableCollection(node);
  }
};
```

If `enableCollection()` is never called on `q_`, PairDefinition can be deferred until the user
confirms they plan to collect it.

### Example 5: No operator<< exists

If there is no ostream operator, propose a starter PairDefinition from obvious getters and ask for
confirmation:

```cpp
class MissInfoPairDef;
class MissInfo
{
public:
  using SpartaPairDefinitionType = MissInfoPairDef;
  uint64_t getRAddr() const;
  uint32_t getLength() const;
  bool isPrefetch() const;
};

class MissInfoPairDef : public sparta::PairDefinition<MissInfo>
{
public:
  MissInfoPairDef() : PairDefinition<MissInfo>()
  {
    SPARTA_INVOKE_PAIRS(MissInfo);
  }

  SPARTA_REGISTER_PAIRS(
    SPARTA_ADDPAIR("raddr", &MissInfo::getRAddr, std::ios::hex),
    SPARTA_ADDPAIR("len", &MissInfo::getLength),
    SPARTA_ADDPAIR("pf", &MissInfo::isPrefetch))
};
```

Ask the user:

- Is `raddr` expected to be hex (assumed yes)?
- Should a `DID` field be added (for example from UID if available)?
- Are there nested members that should be flattened instead?

## Quick Mechanical Mapping Rules

Use these direct mappings when translating legacy ostream code:

- `"name(" << obj.getX() << ")"` maps to `SPARTA_ADDPAIR("name", &Type::getX)`.
- Legacy `std::hex` or explicit `0x` formatting maps to `SPARTA_ADDPAIR(..., std::ios::hex)`.
- `os << *obj.getNestedPtr()` or `os << obj.getNested()` for collected nested objects maps to
  `SPARTA_FLATTEN(&Type::getNestedPtr)` or `SPARTA_FLATTEN(&Type::getNested)`.
- If both nested and scalar fields are printed, use both `SPARTA_FLATTEN` and `SPARTA_ADDPAIR`.
- Order of `SPARTA_FLATTEN` and `SPARTA_ADDPAIR` does not matter.

#### Simple scalar fields

If the ostream operator prints explicit labels and scalar getters, map each label to a field:

```cpp
os << "uid(" << inst.getUID() << ") ";
os << "opc(" << inst.getOpcode() << ") ";
return os << "mnemonic(" << inst.getMnemonic() << ")";
```

This becomes:

```cpp
SPARTA_REGISTER_PAIRS(
    SPARTA_ADDPAIR("uid", &Inst::getUID),
    SPARTA_ADDPAIR("opc", &Inst::getOpcode, std::ios::hex),
    SPARTA_ADDPAIR("mnemonic", &Inst::getMnemonic))
```

#### Flattening a nested collected object

If the ostream operator simply forwards to another object, flatten that getter:

```cpp
return os << *outer.getInst();
```

This becomes:

```cpp
SPARTA_REGISTER_PAIRS(
    SPARTA_FLATTEN(&OuterWithCollectedInst::getInst))
```

#### Mixing flatten and scalar fields

If the type prints a nested object and then adds its own fields, flatten the nested getter and add
the local scalar fields normally. Flatten does not need to come first.

## How To Infer Field Names

Prefer field names already implied by the legacy ostream operator or existing simulator PairDefinitions.

- Reuse existing labels when they are already meaningful and valid.
- Use short, stable names that match the simulator's current conventions.
- Field names must be Python-safe because the UI expects them to be pythonable identifiers or at
  least safe labels.
- Avoid names with spaces, punctuation such as `%` or `$`, or names that begin with digits.

If the codebase already has PairDefinitions for similar types, mirror those naming choices.

## Hex Versus Decimal

Use the legacy ostream formatting as the primary signal.

- If the ostream code used `std::hex` or emitted `0x`-style output, add `std::ios::hex` to the
  corresponding `SPARTA_ADDPAIR`.
- If the field is obviously an address-like value such as `getRAddr()`, `getVAddr()`,
  `getTargetRAddr()`, `getPC()`, or a similar register or address field, assume hex unless the
  surrounding code clearly says otherwise.
- If there is no ostream operator and no obvious formatting clue, make a best guess and ask the user
  whether the field should be hex or decimal.

## DID Fields

Many simulator PairDefinitions include a `DID` field for display ID / color hashing.

- `DID` is not a visible UI field; it is used for auto-colorization.
- It is okay to omit `DID` if there is no obvious stable field to use.
- If omitted, the UI will use the first field as the hash source.
- If there is an obvious stable identifier such as `uid`, use that as `DID` or suggest it to the user.
- If there are multiple plausible choices, ask the user which one should be the display ID. If they
  decide not to use one, let them know that the first field will be used to color their data. Warn
  against using a floating-point field as the DID, and politely push back. Floats are not a good
  DID choice in almost all scenarios. It's not invalid though; just let the user know.

## What To Do When There Is No operator<<

When no legacy ostream operator exists, do not invent an exact field list unless the type is obvious.
Instead:

1. Inspect the getters and members that appear to represent durable state.
2. Propose a reasonable set of collectible fields.
3. Call out any fields that look like obvious hex candidates, obvious display IDs, or nested objects
   that should be flattened.
4. Ask the user to confirm or edit the list before generating final code.

## What Not To Change

- Do not remove existing ostream operators unless the user asks.
- Do not assume the ostream operator is dead code.
- Do not add comments to the generated PairDefinition unless the surrounding file already uses that
  style for nearby PairDefinitions.
- Do not reformat unrelated code just to satisfy the new PairDefinition unless the repo's formatting
  rules clearly require it.

## Practical Checklist For An AI Port

Before writing the PairDefinition, check:

- Is the collected type actually struct-like after pointer normalization?
- Is it already a PairDefinition somewhere else in the repo?
- Does the existing ostream operator reveal likely field names and ordering?
- Are any getters returning nested collected objects that should be flattened?
- Are any fields obviously hex, especially addresses and instruction pointers?
- Should a `DID` field be added for display coloring?

## Existing PairDefinitions As Style Reference

When a codebase already contains production PairDefinitions, treat those files as the local style
reference for constructor shape, `SPARTA_INVOKE_PAIRS`, mixed `SPARTA_ADDPAIR` / `SPARTA_FLATTEN`
usage, naming conventions, and optional `DID` usage.

## Good Default Behavior For The AI

When porting a type, the AI should:

- Prefer a minimal, accurate PairDefinition over a speculative one.
- Reuse the legacy ostream labels (field names) when they are present and sensible.
- Ask questions for ambiguous format or field-selection decisions.
- Keep the code aligned with the file's existing style and existing PairDefinitions.

If the AI cannot confidently choose the field list, hex formatting, or DID, it should stop and ask
for confirmation rather than guessing.

Consider asking the user up-front if they would like a full table containing all
of the types that will be switched over to PairDefinition, their proposed fields,
DID selection, and hex/dec formatting. This will reduce the number of times the
AI has to pause and ask the user another clarifying question.

## External Reference

- If you need some real-world examples, use the open-source Olympia simulator here:
  https://github.com/riscv-software-src/riscv-perf-model
  Olympia already uses PairDefinition in numerous places and can be used as a reference.
- Use Olympia to confirm naming conventions, field ordering, `DID` usage, `SPARTA_FLATTEN` versus
  `SPARTA_ADDPAIR`, and how a real simulator organizes its PairDefinition classes.
- Search Olympia for `SPARTA_REGISTER_PAIRS`, `SPARTA_ADDPAIR`, `SPARTA_FLATTEN`,
  `SpartaPairDefinitionType`, and `enableCollection()` to find directly comparable examples.
- Treat Olympia as a pattern reference, not an exact code template; if the repository cannot be
  accessed or the relevant files are not obvious, ask the user for a local clone or an example file.

