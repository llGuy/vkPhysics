#pragma once

namespace vk {

void prepare_scene3d_data();
// Make sure to update the data that is on the GPU with the new data
// (e.g. new camera transforms, etc...)
void gpu_sync_scene3d_data();

}
