#pragma once

#include "common/containers.hpp"
#include "tools.hpp"
#include "constant.hpp"
#include <bits/stdint-uintn.h>


// Terraforms at a position that was specified in the terraform package
bool terraform(terraform_type_t type, terraform_package_t package, float radius, float speed, float dt);

