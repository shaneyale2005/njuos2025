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
    FILE *fp = fopen(filename, 'r');
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
    Position pos = {-1, -1};
    return pos;
}

bool isEmptySpace(Labyrinth *labyrinth, int row, int col) {
    // TODO: Implement this function
    return false;
}

bool movePlayer(Labyrinth *labyrinth, char playerId, const char *direction) {
    // TODO: Implement this function
    return false;
}

bool saveMap(Labyrinth *labyrinth, const char *filename) {
    // TODO: Implement this function
    return false;
}

// Check if all empty spaces are connected using DFS
void dfs(Labyrinth *labyrinth, int row, int col, bool visited[MAX_ROWS][MAX_COLS]) {
    // TODO: Implement this function
}

bool isConnected(Labyrinth *labyrinth) {
    // TODO: Implement this function
    return false;
}
