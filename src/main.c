#include <common.h>

int main(int argc, char *argv[]) {
    // 初始化日志文件
    log_init("/home/zeehlin/Ass-Igned/Ass-Igned.log");

    // 测试 Log 宏
    Log("This is a test log message: %d", 123);

    // 测试 log_write 宏
    log_write("Direct log_write message: %s\n", "Hello log!");

    // 测试 Assert 宏（会触发断言）
    int a = 5;
    Assert(a == 5, "a should be 5, got %d", a); // 正常，不会触发
    // Assert(a == 0, "a should be 0, got %d", a); // 会触发断言并打印红色错误信息

    // 测试 panic 宏（会触发断言）
    // panic("Something went terribly wrong!"); // 如果需要测试可取消注释

    // TODO 宏测试（会触发 panic）
    // TODO(); // 如果需要测试可取消注释

    return 0;
}
