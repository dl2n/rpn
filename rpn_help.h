Keys:
  0..9      Normal Digit Keys
  A..F      Hex Digit Keys (only active during Hex Base mode)
  <RET>     Enters current value to bottom level of stack.
            Pushes stack up one.
  K         Kilo shorthand, ie, in base 10, 64K = 65536
  S         Change sign (ie, X gets -X)
  R         Roll down stack
  <TAB>     Swap X and Y (bottow two levels of stack)
  <DEL>     Clear X, Pop X from stack
  <BKS>     Backspace
  &         Logical AND operation (X <- Y AND X)
  |         Logical OR operation (X <- Y OR X)
  ^         Logical XOR operation (X <- Y XOR X)
            FLOAT: Y^X operation (X <- Y^X)
  <         Logical LeftShift operation (X <- X << Y)
            Shifts only use the last 6 bits of the Y value.
  >         Logical RightShift operation (X <- X >> Y)
            Shifts only use the last 6 bits of the Y value.
  ~         Logical NOT operation (X <- NOT X)
  +         Addition operator (X <- Y + X)
  -         Subtraction operator (X <- Y - X)
  *         Multiplication operator (X <- Y * X)
  /         Division operator (X <- Y / X)
  %         Modulus operator (X <- Y % X)
  L         Last Base shorthand.  Toggles between the last two bases used.

Base menu:
  2, 8, 10 and 16 are selectable.

Word Size menu:
  8, 16, 32, and 64 are selectable.

Integer menu:
  Two's Complement Signed integers and Unsigned integers are selectable.

The display:
  The top line (in green) is the status display, including current Base,
  Word Size and Integer Type values.

  The next four lines are the T, Z, Y and X stack registers, with T at the top
  and X on the bottom.

