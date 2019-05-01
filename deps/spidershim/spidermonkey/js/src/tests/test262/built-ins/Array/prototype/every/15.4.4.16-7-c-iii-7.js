// Copyright (c) 2012 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-array.prototype.every
es5id: 15.4.4.16-7-c-iii-7
description: >
    Array.prototype.every - return value of callbackfn is a nunmber
    (value is -0)
---*/

        var accessed = false;

        function callbackfn(val, idx, obj) {
            accessed = true;
            return -0;
        }

assert.sameValue([11].every(callbackfn), false, '[11].every(callbackfn)');
assert(accessed, 'accessed !== true');

reportCompare(0, 0);
