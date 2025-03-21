

# CS:APP Data Lab 实验报告

# Github


## 整数运算题

### 1. bitXor
实现异或运算，只能用 `~` 和 `&`。用德摩根定律把 OR 转成 AND：
```c
(~x&y)|(x&~y) => ~(~(~x&y)&~(x&~y))
```

### 2. tmin
返回最小的补码整数，就是把最高位置1：
```c
1 << 31
```

### 3. isTmax
判断是否是最大补码整数。关键是利用 Tmax+1 = Tmin 的特性，同时要排除 -1 这个特例：
```c
!(~(x^(x+1))) & !!(x+1)
```

### 4. allOddBits
检查所有奇数位是否为1。构造掩码 0xAAAAAAAA，然后跟输入按位与比较：
```c
mask = 0xAA << 24 | 0xAA << 16 | 0xAA << 8 | 0xAA
```

### 5. negate
不用减号求相反数。补码的性质：取反加1：
```c
~x + 1
```

### 6. isAsciiDigit
判断是否是 ASCII 数字（0x30-0x39）。分两步：
- 检查高4位是否是 0x3
- 检查低4位是否 ≤ 9

### 7. conditional
实现三目运算符。把 x 转成全0或全1的掩码，用位运算选择 y 或 z：
```c
x = !!x;  // 变成 0 或 1
x = ~x+1; // 变成全0或全1
(x&y)|(~x&z)
```

### 8. isLessOrEqual
判断 x ≤ y。分两种情况：
- 符号不同：x 为负，y 为正
- 符号相同：计算 y-x 是否非负

### 9. logicalNeg
实现逻辑非运算。利用 0 的特性：只有 0 的相反数还是 0。

### 10. howManyBits
计算表示一个数最少需要多少位。用二分查找的思路找最高的 1：
- 先根据符号统一转成正数处理
- 分别检查高16位、8位、4位、2位、1位是否有1

## 浮点数题

### 1. floatScale2
浮点数乘2。分三种情况：
- 非规格化数：尾数左移
- 规格化数：指数加1
- 特殊值（NaN等）：直接返回

### 2. floatFloat2Int
浮点数转整数。主要处理：
- 提取符号位、指数、尾数
- 判断是否溢出
- 根据指数调整尾数位置

### 3. floatPower2
计算2的幂。关键是处理指数范围：
- 太小返回0
- 太大返回无穷
- 正常情况左移23位
