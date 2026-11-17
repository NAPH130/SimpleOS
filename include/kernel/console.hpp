#pragma once

// 支持 %s、%c、%d、%i、%u、%x、%X、%p 和 %%；不支持宽度或长度修饰。
// 调用前须初始化串口；暂不支持并发调用。
void kprintf(const char* format, ...);
