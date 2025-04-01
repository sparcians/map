
# Contributing to Sparta: Guidelines

Bug fixes, enhancements are welcomed and encouraged. But there are a
few rules...

## Rules for Code Development

* Rule1: Any bug fix/enhancement _must be accompanied_ with a test
  that illustrates the bug fix/enhancement.  No test == no acceptance.
  Documentation fixes obviously don't require this...

* Rule2: Adhere to [Sparta's Coding
  Style](sparta-coding-style). Look at the existing code and mimic
  it.  Don't mix another preferred style with Sparta's.  Stick with
  Sparta's.

## Sparta Coding Style

* There are simple style rules:
     1. Class names are `CamelCase` with the Camel's head up: `class SpartaHandler`
     1. Public class method names are `camelCase` with the camel's head down: `void myMethod()`
     1. Private/protected class method names are `camelCase_` with the camel's head down and a trailing `_`: `void myMethod_()`
     1. Member variable names that are `private` or `protected` must
        be all `lower_case_` with a trailing `_` so developers know a
        memory variable is private or protected.  Placing an `_`
        between words: preferred.
     1. Header file guards are `#pragma once`
     1. Any function/class from `std` namespace must always be
        explicit: `std::vector` NOT `vector`.  Never use `using
        namespace <blah>;` in *any* header file
     1. Consider using source files for non-critical path code
     1. Try to keep methods short and concise (yes, we break this rule a bit)
     1. Do not go nuts with `auto`.  This `auto foo(const auto & in)` is ... irritating
     1. All public APIs *must be doxygenated*.  No exceptions.
