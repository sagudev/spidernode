// |reftest| skip -- regexp-unicode-property-escapes is not supported
// Copyright 2017 Mathias Bynens. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
author: Mathias Bynens
description: >
  Unicode property escapes for `Script_Extensions=Masaram_Gondi`
info: |
  Generated by https://github.com/mathiasbynens/unicode-property-escapes-tests
  Unicode v10.0.0
  Emoji v5.0 (UTR51)
esid: sec-static-semantics-unicodematchproperty-p
features: [regexp-unicode-property-escapes]
includes: [regExpUtils.js]
---*/

const matchSymbols = buildString({
  loneCodePoints: [
    0x011D3A
  ],
  ranges: [
    [0x011D00, 0x011D06],
    [0x011D08, 0x011D09],
    [0x011D0B, 0x011D36],
    [0x011D3C, 0x011D3D],
    [0x011D3F, 0x011D47],
    [0x011D50, 0x011D59]
  ]
});
testPropertyEscapes(
  /^\p{Script_Extensions=Masaram_Gondi}+$/u,
  matchSymbols,
  "\\p{Script_Extensions=Masaram_Gondi}"
);
testPropertyEscapes(
  /^\p{Script_Extensions=Gonm}+$/u,
  matchSymbols,
  "\\p{Script_Extensions=Gonm}"
);
testPropertyEscapes(
  /^\p{scx=Masaram_Gondi}+$/u,
  matchSymbols,
  "\\p{scx=Masaram_Gondi}"
);
testPropertyEscapes(
  /^\p{scx=Gonm}+$/u,
  matchSymbols,
  "\\p{scx=Gonm}"
);

const nonMatchSymbols = buildString({
  loneCodePoints: [
    0x011D07,
    0x011D0A,
    0x011D3B,
    0x011D3E
  ],
  ranges: [
    [0x00DC00, 0x00DFFF],
    [0x000000, 0x00DBFF],
    [0x00E000, 0x011CFF],
    [0x011D37, 0x011D39],
    [0x011D48, 0x011D4F],
    [0x011D5A, 0x10FFFF]
  ]
});
testPropertyEscapes(
  /^\P{Script_Extensions=Masaram_Gondi}+$/u,
  nonMatchSymbols,
  "\\P{Script_Extensions=Masaram_Gondi}"
);
testPropertyEscapes(
  /^\P{Script_Extensions=Gonm}+$/u,
  nonMatchSymbols,
  "\\P{Script_Extensions=Gonm}"
);
testPropertyEscapes(
  /^\P{scx=Masaram_Gondi}+$/u,
  nonMatchSymbols,
  "\\P{scx=Masaram_Gondi}"
);
testPropertyEscapes(
  /^\P{scx=Gonm}+$/u,
  nonMatchSymbols,
  "\\P{scx=Gonm}"
);

reportCompare(0, 0);
