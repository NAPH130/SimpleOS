#pragma once

namespace x86_64 {

void initialize_segments();
bool on_double_fault_stack(const void* address);

}
