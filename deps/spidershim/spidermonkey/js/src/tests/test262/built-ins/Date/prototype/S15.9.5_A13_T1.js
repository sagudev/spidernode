// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date.prototype has the property "getUTCMonth"
esid: sec-properties-of-the-date-prototype-object
es5id: 15.9.5_A13_T1
description: The Date.prototype has the property "getUTCMonth"
---*/

if(Date.prototype.hasOwnProperty("getUTCMonth") !== true){
  $ERROR('#1: The Date.prototype has the property "getUTCMonth"');
}

reportCompare(0, 0);
