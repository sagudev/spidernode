// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: 1. Evaluate Expression
es5id: 12.13_A3_T1
description: Evaluating boolean expression
---*/

// CHECK#1
var b=true;
try{
  throw b&&false;
}
catch(e){
  if (e!==false) $ERROR('#1: Exception === false(operaton &&). Actual:  Exception ==='+ e );
}

// CHECK#2
var b=true;
try{
  throw b||false;
}
catch(e){
  if (e!==true) $ERROR('#2: Exception === true(operaton ||). Actual:  Exception ==='+ e );
}

// CHECK#3
try{
  throw !false;
}
catch(e){
  if (e!==true) $ERROR('#3: Exception === true(operaton !). Actual:  Exception ==='+ e );
}

// CHECK#4
var b=true;
try{
  throw !(b&&false);
}
catch(e){
  if (e!==true) $ERROR('#4: Exception === true(operaton &&). Actual:  Exception ==='+ e );
}

reportCompare(0, 0);
