// Copyright (C) 2015 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
es6id: 18.2.5
esid: sec-parseint-string-radix
description: >
  parseInt.name is "parseInt".
info: |
  parseInt (string , radix)

  17 ECMAScript Standard Built-in Objects:
    Every built-in Function object, including constructors, that is not
    identified as an anonymous function has a name property whose value
    is a String.

    Unless otherwise specified, the name property of a built-in Function
    object, if it exists, has the attributes { [[Writable]]: false,
    [[Enumerable]]: false, [[Configurable]]: true }.
includes: [propertyHelper.js]
---*/

assert.sameValue(parseInt.name, "parseInt");

verifyNotEnumerable(parseInt, "name");
verifyNotWritable(parseInt, "name");
verifyConfigurable(parseInt, "name");

reportCompare(0, 0);
