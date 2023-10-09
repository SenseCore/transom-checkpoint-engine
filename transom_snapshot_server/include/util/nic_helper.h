/**
 * @file nic_helper.h
 * @author xial-thu (lovenashbest@126.com)
 * @brief
 * @version 0.1
 * @date 2023-09-13
 *
 * @copyright Copyright (c) 2023
 *
 */

#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace util {
/**
 * @brief helps using multi-NIC, automatically offload task to all avaialable NICs
 */
class MultiNicHelper {
private:
    std::vector<std::string> nics_;
    std::map<std::string, uint> busy_;
    std::mutex mu_;

public:
    MultiNicHelper();
    MultiNicHelper(const MultiNicHelper &) = delete;
    MultiNicHelper(MultiNicHelper &&) = delete;
    MultiNicHelper &operator=(const MultiNicHelper &) = delete;
    MultiNicHelper &operator=(MultiNicHelper &&) = delete;

    static MultiNicHelper &Instance() {
        static std::unique_ptr<MultiNicHelper> instance_ptr(new MultiNicHelper());
        return *instance_ptr;
    }

    /**
     * @brief choose the most idle NIC for rdma communication
     *
     * @return device name
     */
    std::string ChooseNic();

    /**
     * @brief mark target NIC as current task finished
     */
    void ReleaseNic(std::string name);
};
} // namespace util
