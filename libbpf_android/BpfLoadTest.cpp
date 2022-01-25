/*
 * Copyright (C) 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <android-base/file.h>
#include <android-base/macros.h>
#include <gtest/gtest.h>
#include <stdlib.h>
#include <unistd.h>
#include <iostream>
#include "bpf/BpfMap.h"
#include "bpf/BpfUtils.h"
#include "include/libbpf_android.h"

using ::testing::TestWithParam;;

namespace android {
namespace bpf {

class BpfLoadTest : public TestWithParam<std::string> {
  protected:
    BpfLoadTest() {}
    int mProgFd;
    std::string mTpProgPath;
    std::string mTpMapPath;;

    void SetUp() {
        auto progName = android::base::Basename(GetParam());
        progName = progName.substr(0, progName.find_last_of('.'));
        mTpProgPath = "/sys/fs/bpf/prog_" + progName + "_tracepoint_sched_sched_switch";
        unlink(mTpProgPath.c_str());

        mTpMapPath = "/sys/fs/bpf/map_" + progName + "_cpu_pid_map";
        unlink(mTpMapPath.c_str());

        bool critical = true;
        EXPECT_EQ(android::bpf::loadProg(GetParam().c_str(), &critical), 0);
        EXPECT_EQ(false, critical);

        mProgFd = bpf_obj_get(mTpProgPath.c_str());
        EXPECT_GT(mProgFd, 0);

        int ret = bpf_attach_tracepoint(mProgFd, "sched", "sched_switch");
        EXPECT_NE(ret, 0);
    }

    void TearDown() {
        close(mProgFd);
        unlink(mTpProgPath.c_str());
        unlink(mTpMapPath.c_str());
    }

    void checkMapNonZero() {
        // The test program installs a tracepoint on sched:sched_switch
        // and expects the kernel to populate a PID corresponding to CPU
        android::bpf::BpfMap<uint32_t, uint32_t> m(mTpMapPath.c_str());

        // Wait for program to run a little
        sleep(1);

        int non_zero = 0;
        const auto iterFunc = [&non_zero](const uint32_t& key, const uint32_t& val,
                                          BpfMap<uint32_t, uint32_t>& map) {
            if (val && !non_zero) {
                non_zero = 1;
            }

            UNUSED(key);
            UNUSED(map);
            return base::Result<void>();
        };

        EXPECT_RESULT_OK(m.iterateWithValue(iterFunc));
        EXPECT_EQ(non_zero, 1);
    }
};

INSTANTIATE_TEST_SUITE_P(BpfLoadTests, BpfLoadTest,
                         ::testing::Values("/system/etc/bpf/bpf_load_tp_prog.o"));

TEST_P(BpfLoadTest, bpfCheckMap) {
    checkMapNonZero();
}

}  // namespace bpf
}  // namespace android
