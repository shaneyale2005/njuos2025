#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>
#include <testkit.h>
#include "labyrinth.h"

int main(int argc, char *argv[]) {
    // TODO: Implement this function
    return 0;
}

void printUsage() {
    printf("Usage:\n");
    printf("  labyrinth --map map.txt --player id\n");
    printf("  labyrinth -m map.txt -p id\n");
    printf("  labyrinth --map map.txt --player id --move direction\n");
    printf("  labyrinth --version\n");
}

bool isValidPlayer(char playerId) {
    // TODO: Implement this function
    // 这个函数用来判断是否是一个有效的玩家
    return (playerId >= '0'&& playerId <= '9');
}

bool loadMap(Labyrinth *labyrinth, const char *filename) {
    // TODO: Implement this function
    // 注意这里的打开文件的方式，关于文件需要复习
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        return false;
    }
    labyrinth -> rows = 0;
    // 在这里设置一个缓冲区，这很重要，要稍微大一点点
    char buffer [MAX_COLS + 2];

    while (fgets(buffer, sizeof(buffer), fp)) {
        // 下面这段逻辑是用来去掉末尾的换行符的
        int len = strlen(buffer);
        if (buffer[len - 1] == '\n') {
            buffer[len - 1] = '\0';
            len -= 1;
        }

        // 接下来的操作是拷贝到map中
        strcpy(labyrinth->map[labyrinth->rows], buffer);
        labyrinth -> rows ++;
        labyrinth -> cols = len; // 这里是假设所有行的长度都一样的
    }

    fclose(fp);
    return (labyrinth -> rows > 0);
}

Position findPlayer(Labyrinth *labyrinth, char playerId) {
    // TODO: Implement this function
    Position pos = {-1, -1};
    // 可以直接进行判断，前提是需要熟悉Labyrinth这个数据结构
    for (int i = 0; i < labyrinth -> rows; i++) {
        for (int j = 0; j < labyrinth -> cols; j++) {
            if (playerId == labyrinth -> map[i][j]) {
                pos.row = i;
                pos.col = j;
                return pos;
            }
        }
    }
    return pos;
}

Position findFirstEmptySpace(Labyrinth *labyrinth) {
    // TODO: Implement this function
    // 这个函数用来找到第一个空闲的地方，返回二维坐标
    Position pos = {-1, -1};
    for (int i = 0; i < labyrinth -> rows; i++) {
        for (int j = 0; j < labyrinth -> cols; j++) {
            if (labyrinth -> map[i][j] == '.') {
                pos.row = i;
                pos.col = j;
                return pos;
            }
        }
    }
    return pos;
}

bool isEmptySpace(Labyrinth *labyrinth, int row, int col) {
    // TODO: Implement this function
    // 这个函数用来判断一个地方是否是空的，返回真或假
    return (labyrinth -> map[row][col] == '.');
}

bool movePlayer(Labyrinth *labyrinth, char playerId, const char *direction) {
    // TODO: Implement this function
    Position cur_pos = findPlayer(labyrinth, playerId);
    int new_row = cur_pos.row;
    int new_col = cur_pos.col;

    // 主要的逻辑
    if (strcmp(direction, "left") == 0) {
        new_col -= 1;
    } else if (strcmp(direction, "right") == 0) {
        new_col += 1;
    } else if (strcmp(direction, "up") == 0) {
        new_row -= 1;
    } else if (strcmp(direction, "down") == 0) {
        new_row += 1;
    } else {
        return false;
    }
    
    // 进行越界检查
    if (new_row < 0 || new_row >= labyrinth -> rows || new_col < 0 || new_col >= labyrinth -> cols) {
        return false;
    }

    // 看能不能走过去
    if (!isEmptySpace(labyrinth, new_row, new_col)) {
        return false;
    }

    // 最后进行移动
    labyrinth -> map[cur_pos.row][cur_pos.col] = '.';
    labyrinth -> map[new_row][new_col] = playerId;
    return true;
}

bool saveMap(Labyrinth *labyrinth, const char *filename) {
    // TODO: Implement this function
    // 本函数用来保存一个地图，和loadMap的功能恰好相反
    // 注意复习C语言中文件的写入方式
    FILE *fp = fopen(filename, 'w');
    if (!fp) {
        return false; // 打开文件失败
    }
    // 这里逐行写入迷宫
    for (int i = 0; i < labyrinth -> rows; i++) {
        fprintf(fp, "%s\n", labyrinth -> map[i]);
    }

    fclose(fp);
    return true;
}

// Check if all empty spaces are connected using DFS
void dfs(Labyrinth *labyrinth, int row, int col, bool visited[MAX_ROWS][MAX_COLS]) {
    // TODO: Implement this function
    // 首先应该是边界条件的检查，建议复习dfs的模版函数
    if (row < 0 || row >= MAX_ROWS || col < 0 || col >= MAX_COLS || labyrinth -> map[row][col] != '.' || visited[row][col]) {
        return;
    }
    visited[row][col] = true;
    dfs(labyrinth, row - 1, col, visited);
    dfs(labyrinth, row + 1, col, visited);
    dfs(labyrinth, row, col - 1, visited);
    dfs(labyrinth, row, col + 1, visited);
}

bool isConnected(Labyrinth *labyrinth) {
    // TODO: Implement this function
    // 这个函数是用来检查整体的连通性的
    bool visited[MAX_ROWS][MAX_COLS] = {false};

    Position firstEmpty = findFirstEmptySpace(labyrinth);
    if (firstEmpty.row == -1 && firstEmpty.col == -1) {
        return true;
    }
    dfs(labyrinth, firstEmpty.row, firstEmpty.col, visited);
    // 如果存在dfs没有搜索到的空地，就认为不是连通的
    for (int i = 0; i < MAX_ROWS; i++) {
        for (int j = 0; j < MAX_COLS; j++) {
            if (labyrinth -> map[i][j] == '.' && !visited[i][j]) {
                return false;
            }
        }
    }
    return true;
}
