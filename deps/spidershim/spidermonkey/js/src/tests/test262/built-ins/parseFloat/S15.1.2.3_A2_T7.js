// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: Operator remove leading StrWhiteSpaceChar
es5id: 15.1.2.3_A2_T7
es6id: 18.2.4
esid: sec-parsefloat-string
description: "StrWhiteSpaceChar :: LF (U+000A)"
---*/

//CHECK#1
if (parseFloat("\u000A1.1") !== parseFloat("1.1")) {
  $ERROR('#1: parseFloat("\\u000A1.1") === parseFloat("1.1"). Actual: ' + (parseFloat("\u000A1.1")));
}

//CHECK#2
if (parseFloat("\u000A\u000A-1.1") !== parseFloat("-1.1")) {
  $ERROR('#2: parseFloat("\\u000A\\u000A-1.1") === parseFloat("-1.1"). Actual: ' + (parseFloat("\u000A\u000A-1.1")));
}

//CHECK#3
assert.sameValue(parseFloat("\u000A"), NaN);

reportCompare(0, 0);
