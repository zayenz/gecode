var string: text;
var int: length;
var int: emoji_code;

constraint text = "日本😀";
constraint str_len(text) = length;
constraint str_reg(text, "日本.");
constraint emoji_code = str_char2code("😀");

solve satisfy;

output [
  "length = ", show(length), ";\n",
  "emoji_code = ", show(emoji_code), ";\n",
  "text = ", show(text), ";\n"
];
