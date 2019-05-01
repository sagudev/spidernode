// |reftest| skip -- BigInt is not supported
// Copyright (C) 2017 Robin Templeton. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-multiplicative-operators-runtime-semantics-evaluation
description: BigInt multiplication arithmetic
features: [BigInt]
---*/

function testMul(x, y, z) {
    assert.sameValue(x * y, z, x + " * " + y + " = " + z);
    assert.sameValue(y * x, z, y + " * " + x + " = " + z);
}

testMul(0xFEDCBA9876543210n, 0xFEDCBA9876543210n, 0xFDBAC097C8DC5ACCDEEC6CD7A44A4100n);
testMul(0xFEDCBA9876543210n, 0xFEDCBA987654320Fn, 0xFDBAC097C8DC5ACBE00FB23F2DF60EF0n);
testMul(0xFEDCBA9876543210n, 0xFEDCBA98n, 0xFDBAC097530ECA86541D5980n);
testMul(0xFEDCBA9876543210n, 0xFEDCBA97n, 0xFDBAC09654320FEDDDC92770n);
testMul(0xFEDCBA9876543210n, 0x1234n, 0x121F49F49F49F49F4B40n);
testMul(0xFEDCBA9876543210n, 0x3n, 0x2FC962FC962FC9630n);
testMul(0xFEDCBA9876543210n, 0x2n, 0x1FDB97530ECA86420n);
testMul(0xFEDCBA9876543210n, 0x1n, 0xFEDCBA9876543210n);
testMul(0xFEDCBA9876543210n, 0x0n, 0x0n);
testMul(0xFEDCBA9876543210n, -0x1n, -0xFEDCBA9876543210n);
testMul(0xFEDCBA9876543210n, -0x2n, -0x1FDB97530ECA86420n);
testMul(0xFEDCBA9876543210n, -0x3n, -0x2FC962FC962FC9630n);
testMul(0xFEDCBA9876543210n, -0x1234n, -0x121F49F49F49F49F4B40n);
testMul(0xFEDCBA9876543210n, -0xFEDCBA97n, -0xFDBAC09654320FEDDDC92770n);
testMul(0xFEDCBA9876543210n, -0xFEDCBA98n, -0xFDBAC097530ECA86541D5980n);
testMul(0xFEDCBA9876543210n, -0xFEDCBA987654320Fn, -0xFDBAC097C8DC5ACBE00FB23F2DF60EF0n);
testMul(0xFEDCBA9876543210n, -0xFEDCBA9876543210n, -0xFDBAC097C8DC5ACCDEEC6CD7A44A4100n);
testMul(0xFEDCBA987654320Fn, 0xFEDCBA987654320Fn, 0xFDBAC097C8DC5ACAE132F7A6B7A1DCE1n);
testMul(0xFEDCBA987654320Fn, 0xFEDCBA98n, 0xFDBAC097530ECA8555409EE8n);
testMul(0xFEDCBA987654320Fn, 0xFEDCBA97n, 0xFDBAC09654320FECDEEC6CD9n);
testMul(0xFEDCBA987654320Fn, 0x1234n, 0x121F49F49F49F49F390Cn);
testMul(0xFEDCBA987654320Fn, 0x3n, 0x2FC962FC962FC962Dn);
testMul(0xFEDCBA987654320Fn, 0x2n, 0x1FDB97530ECA8641En);
testMul(0xFEDCBA987654320Fn, 0x1n, 0xFEDCBA987654320Fn);
testMul(0xFEDCBA987654320Fn, 0x0n, 0x0n);
testMul(0xFEDCBA987654320Fn, -0x1n, -0xFEDCBA987654320Fn);
testMul(0xFEDCBA987654320Fn, -0x2n, -0x1FDB97530ECA8641En);
testMul(0xFEDCBA987654320Fn, -0x3n, -0x2FC962FC962FC962Dn);
testMul(0xFEDCBA987654320Fn, -0x1234n, -0x121F49F49F49F49F390Cn);
testMul(0xFEDCBA987654320Fn, -0xFEDCBA97n, -0xFDBAC09654320FECDEEC6CD9n);
testMul(0xFEDCBA987654320Fn, -0xFEDCBA98n, -0xFDBAC097530ECA8555409EE8n);
testMul(0xFEDCBA987654320Fn, -0xFEDCBA987654320Fn, -0xFDBAC097C8DC5ACAE132F7A6B7A1DCE1n);
testMul(0xFEDCBA987654320Fn, -0xFEDCBA9876543210n, -0xFDBAC097C8DC5ACBE00FB23F2DF60EF0n);
testMul(0xFEDCBA98n, 0xFEDCBA98n, 0xFDBAC096DD413A40n);
testMul(0xFEDCBA98n, 0xFEDCBA97n, 0xFDBAC095DE647FA8n);
testMul(0xFEDCBA98n, 0x1234n, 0x121F49F496E0n);
testMul(0xFEDCBA98n, 0x3n, 0x2FC962FC8n);
testMul(0xFEDCBA98n, 0x2n, 0x1FDB97530n);
testMul(0xFEDCBA98n, 0x1n, 0xFEDCBA98n);
testMul(0xFEDCBA98n, 0x0n, 0x0n);
testMul(0xFEDCBA98n, -0x1n, -0xFEDCBA98n);
testMul(0xFEDCBA98n, -0x2n, -0x1FDB97530n);
testMul(0xFEDCBA98n, -0x3n, -0x2FC962FC8n);
testMul(0xFEDCBA98n, -0x1234n, -0x121F49F496E0n);
testMul(0xFEDCBA98n, -0xFEDCBA97n, -0xFDBAC095DE647FA8n);
testMul(0xFEDCBA98n, -0xFEDCBA98n, -0xFDBAC096DD413A40n);
testMul(0xFEDCBA98n, -0xFEDCBA987654320Fn, -0xFDBAC097530ECA8555409EE8n);
testMul(0xFEDCBA98n, -0xFEDCBA9876543210n, -0xFDBAC097530ECA86541D5980n);
testMul(0xFEDCBA97n, 0xFEDCBA97n, 0xFDBAC094DF87C511n);
testMul(0xFEDCBA97n, 0x1234n, 0x121F49F484ACn);
testMul(0xFEDCBA97n, 0x3n, 0x2FC962FC5n);
testMul(0xFEDCBA97n, 0x2n, 0x1FDB9752En);
testMul(0xFEDCBA97n, 0x1n, 0xFEDCBA97n);
testMul(0xFEDCBA97n, 0x0n, 0x0n);
testMul(0xFEDCBA97n, -0x1n, -0xFEDCBA97n);
testMul(0xFEDCBA97n, -0x2n, -0x1FDB9752En);
testMul(0xFEDCBA97n, -0x3n, -0x2FC962FC5n);
testMul(0xFEDCBA97n, -0x1234n, -0x121F49F484ACn);
testMul(0xFEDCBA97n, -0xFEDCBA97n, -0xFDBAC094DF87C511n);
testMul(0xFEDCBA97n, -0xFEDCBA98n, -0xFDBAC095DE647FA8n);
testMul(0xFEDCBA97n, -0xFEDCBA987654320Fn, -0xFDBAC09654320FECDEEC6CD9n);
testMul(0xFEDCBA97n, -0xFEDCBA9876543210n, -0xFDBAC09654320FEDDDC92770n);
testMul(0x1234n, 0x1234n, 0x14B5A90n);
testMul(0x1234n, 0x3n, 0x369Cn);
testMul(0x1234n, 0x2n, 0x2468n);
testMul(0x1234n, 0x1n, 0x1234n);
testMul(0x1234n, 0x0n, 0x0n);
testMul(0x1234n, -0x1n, -0x1234n);
testMul(0x1234n, -0x2n, -0x2468n);
testMul(0x1234n, -0x3n, -0x369Cn);
testMul(0x1234n, -0x1234n, -0x14B5A90n);
testMul(0x1234n, -0xFEDCBA97n, -0x121F49F484ACn);
testMul(0x1234n, -0xFEDCBA98n, -0x121F49F496E0n);
testMul(0x1234n, -0xFEDCBA987654320Fn, -0x121F49F49F49F49F390Cn);
testMul(0x1234n, -0xFEDCBA9876543210n, -0x121F49F49F49F49F4B40n);
testMul(0x3n, 0x3n, 0x9n);
testMul(0x3n, 0x2n, 0x6n);
testMul(0x3n, 0x1n, 0x3n);
testMul(0x3n, 0x0n, 0x0n);
testMul(0x3n, -0x1n, -0x3n);
testMul(0x3n, -0x2n, -0x6n);
testMul(0x3n, -0x3n, -0x9n);
testMul(0x3n, -0x1234n, -0x369Cn);
testMul(0x3n, -0xFEDCBA97n, -0x2FC962FC5n);
testMul(0x3n, -0xFEDCBA98n, -0x2FC962FC8n);
testMul(0x3n, -0xFEDCBA987654320Fn, -0x2FC962FC962FC962Dn);
testMul(0x3n, -0xFEDCBA9876543210n, -0x2FC962FC962FC9630n);
testMul(0x2n, 0x2n, 0x4n);
testMul(0x2n, 0x1n, 0x2n);
testMul(0x2n, 0x0n, 0x0n);
testMul(0x2n, -0x1n, -0x2n);
testMul(0x2n, -0x2n, -0x4n);
testMul(0x2n, -0x3n, -0x6n);
testMul(0x2n, -0x1234n, -0x2468n);
testMul(0x2n, -0xFEDCBA97n, -0x1FDB9752En);
testMul(0x2n, -0xFEDCBA98n, -0x1FDB97530n);
testMul(0x2n, -0xFEDCBA987654320Fn, -0x1FDB97530ECA8641En);
testMul(0x2n, -0xFEDCBA9876543210n, -0x1FDB97530ECA86420n);
testMul(0x1n, 0x1n, 0x1n);
testMul(0x1n, 0x0n, 0x0n);
testMul(0x1n, -0x1n, -0x1n);
testMul(0x1n, -0x2n, -0x2n);
testMul(0x1n, -0x3n, -0x3n);
testMul(0x1n, -0x1234n, -0x1234n);
testMul(0x1n, -0xFEDCBA97n, -0xFEDCBA97n);
testMul(0x1n, -0xFEDCBA98n, -0xFEDCBA98n);
testMul(0x1n, -0xFEDCBA987654320Fn, -0xFEDCBA987654320Fn);
testMul(0x1n, -0xFEDCBA9876543210n, -0xFEDCBA9876543210n);
testMul(0x0n, 0x0n, 0x0n);
testMul(0x0n, -0x1n, 0x0n);
testMul(0x0n, -0x2n, 0x0n);
testMul(0x0n, -0x3n, 0x0n);
testMul(0x0n, -0x1234n, 0x0n);
testMul(0x0n, -0xFEDCBA97n, 0x0n);
testMul(0x0n, -0xFEDCBA98n, 0x0n);
testMul(0x0n, -0xFEDCBA987654320Fn, 0x0n);
testMul(0x0n, -0xFEDCBA9876543210n, 0x0n);
testMul(-0x1n, -0x1n, 0x1n);
testMul(-0x1n, -0x2n, 0x2n);
testMul(-0x1n, -0x3n, 0x3n);
testMul(-0x1n, -0x1234n, 0x1234n);
testMul(-0x1n, -0xFEDCBA97n, 0xFEDCBA97n);
testMul(-0x1n, -0xFEDCBA98n, 0xFEDCBA98n);
testMul(-0x1n, -0xFEDCBA987654320Fn, 0xFEDCBA987654320Fn);
testMul(-0x1n, -0xFEDCBA9876543210n, 0xFEDCBA9876543210n);
testMul(-0x2n, -0x2n, 0x4n);
testMul(-0x2n, -0x3n, 0x6n);
testMul(-0x2n, -0x1234n, 0x2468n);
testMul(-0x2n, -0xFEDCBA97n, 0x1FDB9752En);
testMul(-0x2n, -0xFEDCBA98n, 0x1FDB97530n);
testMul(-0x2n, -0xFEDCBA987654320Fn, 0x1FDB97530ECA8641En);
testMul(-0x2n, -0xFEDCBA9876543210n, 0x1FDB97530ECA86420n);
testMul(-0x3n, -0x3n, 0x9n);
testMul(-0x3n, -0x1234n, 0x369Cn);
testMul(-0x3n, -0xFEDCBA97n, 0x2FC962FC5n);
testMul(-0x3n, -0xFEDCBA98n, 0x2FC962FC8n);
testMul(-0x3n, -0xFEDCBA987654320Fn, 0x2FC962FC962FC962Dn);
testMul(-0x3n, -0xFEDCBA9876543210n, 0x2FC962FC962FC9630n);
testMul(-0x1234n, -0x1234n, 0x14B5A90n);
testMul(-0x1234n, -0xFEDCBA97n, 0x121F49F484ACn);
testMul(-0x1234n, -0xFEDCBA98n, 0x121F49F496E0n);
testMul(-0x1234n, -0xFEDCBA987654320Fn, 0x121F49F49F49F49F390Cn);
testMul(-0x1234n, -0xFEDCBA9876543210n, 0x121F49F49F49F49F4B40n);
testMul(-0xFEDCBA97n, -0xFEDCBA97n, 0xFDBAC094DF87C511n);
testMul(-0xFEDCBA97n, -0xFEDCBA98n, 0xFDBAC095DE647FA8n);
testMul(-0xFEDCBA97n, -0xFEDCBA987654320Fn, 0xFDBAC09654320FECDEEC6CD9n);
testMul(-0xFEDCBA97n, -0xFEDCBA9876543210n, 0xFDBAC09654320FEDDDC92770n);
testMul(-0xFEDCBA98n, -0xFEDCBA98n, 0xFDBAC096DD413A40n);
testMul(-0xFEDCBA98n, -0xFEDCBA987654320Fn, 0xFDBAC097530ECA8555409EE8n);
testMul(-0xFEDCBA98n, -0xFEDCBA9876543210n, 0xFDBAC097530ECA86541D5980n);
testMul(-0xFEDCBA987654320Fn, -0xFEDCBA987654320Fn, 0xFDBAC097C8DC5ACAE132F7A6B7A1DCE1n);
testMul(-0xFEDCBA987654320Fn, -0xFEDCBA9876543210n, 0xFDBAC097C8DC5ACBE00FB23F2DF60EF0n);
testMul(-0xFEDCBA9876543210n, -0xFEDCBA9876543210n, 0xFDBAC097C8DC5ACCDEEC6CD7A44A4100n);

reportCompare(0, 0);
