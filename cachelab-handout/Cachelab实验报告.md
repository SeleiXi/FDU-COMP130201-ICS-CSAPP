# Cachelab 实验报告

## 实验概览

本实验包含两个部分：Part A 用于实现一个高速缓存模拟器，Part B 用于实现缓存友好的矩阵转置算法。

本实验的高速缓存使用组相连映射方式进行地址映射，采用 LRU (Least Recently Used) 替换策略。Part A 需要实现缓存的内存分配、加载(L)、存储(S)、修改(M)等基本操作。Part B 需要针对不同尺寸的矩阵设计缓存友好的转置算法，以最小化缓存缺失次数。

缓存参数：s（组索引位数）、E（每组行数）、b（块偏移位数）
- 缓存大小：1KB
- 块大小：32字节 
- 映射方式：直接映射（E=1）或组相连映射
- 替换策略：LRU

## Part A - 缓存模拟器实现

### 数据结构设计

**缓存行结构**：
```c
typedef struct cache_line {
    int valid;      // 有效位
    int tag;        // 标记位
    int time;       // 时间戳，用于LRU替换策略（最近使用=0）
} Cache_line;
```

**缓存结构**：
```c
typedef struct cache_ {
    int B;          // 块大小 (2^b)
    int E;          // 组内行数（相连度）
    int S;          // 组数 (2^s)
    Cache_line **line;  // 二维缓存行数组
} Cache;
```

### 核心功能实现

**1. 缓存初始化**：
```c
void init_cache(int s, int E, int b) {
    int S = 1 << s;  // 组数 = 2^s
    int B = 1 << b;  // 块大小 = 2^b
    
    cache = (Cache*)malloc(sizeof(Cache));
    cache->S = S;
    cache->E = E;
    cache->B = B;
    
    // 为每个组分配内存
    cache->line = (Cache_line**)malloc(sizeof(Cache_line*) * S);
    for (int i = 0; i < S; ++i) {
        cache->line[i] = (Cache_line*)malloc(sizeof(Cache_line) * E);
        for (int j = 0; j < E; ++j) {
            cache->line[i][j].valid = 0;
            cache->line[i][j].tag = -1;
            cache->line[i][j].time = 0;
        }
    }
}
```

**2. 缓存访问模拟**：
```c
void access_cache(int set_idx, int tag) {
    int line_idx = find_line(tag, set_idx);
    
    if (line_idx == -1) {  // 缓存缺失
        miss_cnt++;
        if (verbose == 1) printf("miss ");
        
        int empty_line = find_empty_line(set_idx);
        if (empty_line == -1) {  // 需要替换
            eviction_cnt++;
            if (verbose == 1) printf("eviction ");
            empty_line = find_lru_line(set_idx);
        }
        update_time_tag(empty_line, set_idx, tag);
    } else {  // 缓存命中
        hit_cnt++;
        if (verbose == 1) printf("hit ");
        update_time_tag(line_idx, set_idx, tag);
    }
}
```

**3. LRU替换策略**：
```c
int find_lru_line(int set_idx) {
    int max_line = 0, max_time = 0;
    for (int i = 0; i < cache->E; ++i) {
        if (cache->line[set_idx][i].time > max_time) {
            max_time = cache->line[set_idx][i].time;
            max_line = i;
        }
    }
    return max_line;
}
```

**4. 时间戳更新**：
```c
void update_time_tag(int line_idx, int set_idx, int tag) {
    cache->line[set_idx][line_idx].valid = 1;
    cache->line[set_idx][line_idx].tag = tag;
    
    // 所有有效行的时间戳加1
    for (int i = 0; i < cache->E; ++i) {
        if (cache->line[set_idx][i].valid == 1) {
            cache->line[set_idx][i].time++;
        }
    }
    
    // 当前访问行设为最近使用（时间戳=0）
    cache->line[set_idx][line_idx].time = 0;
}
```

**5. 指令解析**：
```c
void simulate_cache(int s, int E, int b) {
    FILE *file = fopen(trace_file, "r");
    char operation;
    unsigned address;
    int size;
    
    while (fscanf(file, " %c %x,%d", &operation, &address, &size) > 0) {
        // 提取标记位和组索引
        int tag = address >> (s + b);
        int set_idx = (address >> b) & (((unsigned)(-1)) >> (8 * sizeof(unsigned) - s));
        
        switch (operation) {
            case 'M':  // 修改：加载+存储
                access_cache(set_idx, tag);
                access_cache(set_idx, tag);
                break;
            case 'L':  // 加载
                access_cache(set_idx, tag);
                break;
            case 'S':  // 存储
                access_cache(set_idx, tag);
                break;
        }
    }
    fclose(file);
}
```

### Part A 测试结果
```
Part A: Testing cache simulator
                        Your simulator     Reference simulator
Points (s,E,b)    Hits  Misses  Evicts    Hits  Misses  Evicts
     3 (1,1,1)       9       8       6       9       8       6  traces/yi2.trace
     3 (4,2,4)       4       5       2       4       5       2  traces/yi.trace
     3 (2,1,4)       2       3       1       2       3       1  traces/dave.trace
     3 (2,1,3)     167      71      67     167      71      67  traces/trans.trace
     3 (2,2,3)     201      37      29     201      37      29  traces/trans.trace
     3 (2,4,3)     212      26      10     212      26      10  traces/trans.trace
     3 (5,1,5)     231       7       0     231       7       0  traces/trans.trace
     6 (5,1,5)  265189   21775   21743  265189   21775   21743  traces/long.trace
    27
```


## Part B - 矩阵转置优化

Part B 需要在指定的缓存配置下（S=32, E=1, b=5，即32组直接映射，每块32字节）实现三种不同尺寸矩阵的高效转置。

### 缓存分析
- 缓存大小：1KB = 32组 × 32字节/组
- 每个缓存块可存储：32字节 ÷ 4字节/int = 8个整数
- 对于矩阵A[N][M]，第i行第j列的地址：&A[i][j] = base + (i×M + j)×4

### 32×32 矩阵转置优化

**问题分析**：
- 矩阵大小：32×32 = 1024个元素
- 直接转置会导致大量缓存冲突，因为A[i][j]和B[j][i]可能映射到同一缓存组

**解决方案 - 8×8分块**：
```c
void transpose_32x32(int M, int N, int A[N][M], int B[M][N]) {
    int i, j, k;
    int a0, a1, a2, a3, a4, a5, a6, a7;
    
    // 将32×32矩阵分成16个8×8子块
    for (i = 0; i < 32; i += 8) {
        for (j = 0; j < 32; j += 8) {
            // 转置每个8×8子块
            for (k = i; k < i + 8; ++k) {
                // 一次读取A的一整行（8个元素）
                a0 = A[k][j];     a1 = A[k][j + 1];
                a2 = A[k][j + 2]; a3 = A[k][j + 3];
                a4 = A[k][j + 4]; a5 = A[k][j + 5];
                a6 = A[k][j + 6]; a7 = A[k][j + 7];
                
                // 写入B的对应列位置
                B[j][k] = a0;     B[j + 1][k] = a1;
                B[j + 2][k] = a2; B[j + 3][k] = a3;
                B[j + 4][k] = a4; B[j + 5][k] = a5;
                B[j + 6][k] = a6; B[j + 7][k] = a7;
            }
        }
    }
}
```

**优化效果**：缺失次数从1184降至288，远低于300的要求。

### 64×64 矩阵转置优化

**问题分析**：
- 64×64矩阵更容易产生缓存冲突
- 简单的8×8分块效果不佳

**解决方案 - 复杂8×8分块策略**：
将每个8×8块分成4个4×4子块，采用三步策略：

```c
void transpose_64x64(int M, int N, int A[N][M], int B[M][N]) {
    int i, j, k;
    int a0, a1, a2, a3, a4, a5, a6, a7;
    
    for (i = 0; i < 64; i += 8) {
        for (j = 0; j < 64; j += 8) {
            // 步骤1：处理上半部分（左上+右上）
            for (k = i; k < i + 4; ++k) {
                a0 = A[k][j];     a1 = A[k][j + 1];
                a2 = A[k][j + 2]; a3 = A[k][j + 3];
                a4 = A[k][j + 4]; a5 = A[k][j + 5];
                a6 = A[k][j + 6]; a7 = A[k][j + 7];
                
                // 左上4×4：正确位置
                B[j][k] = a0;     B[j + 1][k] = a1;
                B[j + 2][k] = a2; B[j + 3][k] = a3;
                
                // 右上4×4：临时存储在错误位置
                B[j][k + 4] = a4;     B[j + 1][k + 4] = a5;
                B[j + 2][k + 4] = a6; B[j + 3][k + 4] = a7;
            }
            
            // 步骤2：处理左下部分并移动右上部分
            for (k = j; k < j + 4; ++k) {
                // 读取临时存储的右上部分
                a0 = B[k][i + 4]; a1 = B[k][i + 5];
                a2 = B[k][i + 6]; a3 = B[k][i + 7];
                
                // 读取左下4×4
                a4 = A[i + 4][k]; a5 = A[i + 5][k];
                a6 = A[i + 6][k]; a7 = A[i + 7][k];
                
                // 左下4×4放到正确位置
                B[k][i + 4] = a4; B[k][i + 5] = a5;
                B[k][i + 6] = a6; B[k][i + 7] = a7;
                
                // 右上4×4移到正确位置
                B[k + 4][i] = a0;     B[k + 4][i + 1] = a1;
                B[k + 4][i + 2] = a2; B[k + 4][i + 3] = a3;
            }
            
            // 步骤3：处理右下4×4
            for (k = i + 4; k < i + 8; ++k) {
                a4 = A[k][j + 4]; a5 = A[k][j + 5];
                a6 = A[k][j + 6]; a7 = A[k][j + 7];
                
                B[j + 4][k] = a4; B[j + 5][k] = a5;
                B[j + 6][k] = a6; B[j + 7][k] = a7;
            }
        }
    }
}
```

**优化效果**：缺失次数1228，低于1300的要求。

### 61×67 矩阵转置优化

**问题分析**：
- 非规则矩阵尺寸
- 8×8分块会有边界处理问题

**解决方案 - 16×16分块**：
```c
void transpose_61x67(int M, int N, int A[N][M], int B[M][N]) {
    int i, j, t, s;
    
    // 使用16×16分块提高缓存利用率
    for (i = 0; i < N; i += 16) {
        for (j = 0; j < M; j += 16) {
            // 处理每个块，注意边界条件
            for (t = i; t < i + 16 && t < N; ++t) {
                for (s = j; s < j + 16 && s < M; ++s) {
                    B[s][t] = A[t][s];
                }
            }
        }
    }
}
```

**优化效果**：缺失次数1993，低于2000的要求。

### 统一接口函数

```c
void transpose_submit(int M, int N, int A[N][M], int B[M][N]) {
    if (M == 32 && N == 32) {
        transpose_32x32(M, N, A, B);
    } else if (M == 64 && N == 64) {
        transpose_64x64(M, N, A, B);
    } else if (M == 61 && N == 67) {
        transpose_61x67(M, N, A, B);
    } else {
        // 其他尺寸的fallback实现
        int i, j, ii, jj, tmp;
        int block_size = 8;
        
        for (ii = 0; ii < N; ii += block_size) {
            for (jj = 0; jj < M; jj += block_size) {
                for (i = ii; i < ii + block_size && i < N; i++) {
                    for (j = jj; j < jj + block_size && j < M; j++) {
                        tmp = A[i][j];
                        B[j][i] = tmp;
                    }
                }
            }
        }
    }
}
```

## 最终测试结果

```
Cache Lab summary:
                        Points   Max pts      Misses
Csim correctness          27.0        27
Trans perf 32x32           8.0         8         288
Trans perf 64x64           8.0         8        1228
Trans perf 61x67          10.0        10        1993
          Total points    53.0        53
```
