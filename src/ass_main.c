#include "common.h"
#include "monitor.h"
#include "ai_client.h"

int main(int argc, char *argv[]) {
  monitor_init(argc, argv);
  // adb_mainloop();

  char *answer = NULL;
  const char *question = "请用中文写一句问候语。";

  printf("Sending question: %s\n", question);

  // 2. 调用核心功能函数
  answer = aic_call(question);
  
  if (answer) {
      printf("\n--- AI Response ---\n");
      printf("Answer: %s\n", answer);
      
      // 3. 必须释放 aic_call 返回的字符串
      free(answer); 
  } else {
      printf("\nCall failed or returned an empty response.\n");
  }

  monitor_cleanup();
  return 0;
}
