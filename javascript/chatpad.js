let serial = require("serial");
serial.setup("lpuart", 19200);
serial.write([0x87, 0x02, 0x8C, 0x1F, 0xCC]); //init
serial.read(1, 1000); // wait for response

let keyMap = [['7', '6', '5', '4', '3', '2', '1'],
['U', 'Y', 'T', 'R', 'E', 'W', 'Q'],
['J', 'H', 'G', 'F', 'D', 'S', 'A'],
['N', 'B', 'V', 'C', 'X', 'Z', '?'],
['\x12', 'M', '.', ' ', '\x11', '?', '?'], // Right (0x12), Left (0x11)
['?', ',', '\0x0D', 'P', '0', '9', '8'], // enter
['\x08', 'L', '?', '?', 'O', 'I', 'K'] // backspace
];

let orangeMap = {"R": "$", "P":"=", ",": ";", "J": "\"", "H": "\\", "V":"_", "B": "+"};
let greenMap = {"Q": "!", "W": "@", "R" : "#", "T": "%", "Y": "^", "U": "&", "I": "*", "O": "(", "P": ")", "A": "~", "D": "{", "F": "}", "H": "/", "J": "'", "K": "[", "L": "]", ",":":", "Z": "`", "V": "-", "B": "|", "N":"<", "M": ">", ".": "?"};

function toChar(code, mod) {
  let row = code >> 4;
  let col = code & 0xF;
  let button = 'x';
  if (row > 0 && col > 0 && row <= keyMap.length && col <= keyMap[row - 1].length) {
    button = keyMap[row - 1][col - 1];
    if (mod === 4) {
      button = orangeMap[button];
    } else if (mod === 2) {
      button = greenMap[button];
    } else if (mod === 0 && button.length === 1) {
      button = button.toLowerCase();
    }
  } else if (code === 0) {
    if (mod === 1) {
      button = 'Shift';
    } else if (mod === 2) {
      button = 'Green';
    } else if (mod === 4) {
      button = 'Orange';
    } else if (mod === 8) {
      button = 'People';
    }
  }
  return button;
}

let line = 0;
let show = true;
let ready = false;
while (true) {
  if (line++ % 100 === 0) {
    serial.write([0x87, 0x02, 0x8C, 0x1B, 0xD0]); //sync
  }

  let chars = serial.read(8, 10);
  if (chars === undefined || chars.length < 8) {
    continue;
  }

  let ch = chars.charCodeAt(0);
  if ((ch !== 0xB4) && (ch !== 0xA5)) {
    let codes = chars.charCodeAt(0).toString(16);
    for (let j = 1; j < chars.length; j++) {
      let ch = chars.charCodeAt(j);
      codes = codes + "," + ch.toString(16);
    }
    console.error("Codes: ", codes);
    continue;
  }

  if (ch === 0xb4 && chars.charCodeAt(1) === 0xc5 && show) {
    let mod = chars.charCodeAt(3);
    let btn = chars.charCodeAt(4);
    if (mod !== 0 || btn !== 0) {
      console.error("mod: ", mod.toString(16), "btn: ", btn.toString(16), "key: ", toChar(btn, mod));
      if (btn !== 0) {
        print(toChar(btn, mod));
      }
    }
    show = false;
  } else if (ch === 0xa5) {
    if (!ready) {
      ready = true;
      console.error("Ready! ", line);
      print("Ready! ", line);
    }
    show = true;
  }
}
