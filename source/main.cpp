#define DoMain
// #define DoTest

#ifdef DoMain

#include "mimgs/Mimgs.hpp"
#include "mimgs/types/EImages.hpp"
#include "mlog/Log.hpp" 

#include <opencv2/opencv.hpp>
#include <iostream>
#include <vector>
#include <unordered_map>
#include <memory>
#include <cstring>

// 辅助函数：读取本地图片并转化为 SDK 通用的 ImageAsset 内存资产
std::shared_ptr<ei::mimgs::ImageAsset> LoadLocalImage(const std::string& filePath, const std::string& assetId)
{
    // 1. 使用 OpenCV 读取磁盘图片（保留原始通道，包括透明度）
    cv::Mat img = cv::imread(filePath, cv::IMREAD_UNCHANGED);
    if (img.empty())
    {
        throw std::runtime_error("Client Error: 无法读取或找不到图片文件 -> " + filePath);
    }

    // 2. 识别通道并进行色彩空间转换（OpenCV 默认是 BGR/BGRA，SDK 契约要求 RGB/RGBA）
    ei::mimgs::PixelFormat fmt;
    if (img.channels() == 4)
    {
        cv::cvtColor(img, img, cv::COLOR_BGRA2RGBA);
        fmt = ei::mimgs::PixelFormat::RGBA8888;
    }
    else if (img.channels() == 3)
    {
        cv::cvtColor(img, img, cv::COLOR_BGR2RGB);
        fmt = ei::mimgs::PixelFormat::RGB888;
    }
    else
    {
        // 如果是单通道灰度图，强制提升为 RGB
        cv::cvtColor(img, img, cv::COLOR_GRAY2RGB);
        fmt = ei::mimgs::PixelFormat::RGB888;
    }

    // 3. 实例化 ImageAsset 并将像素数据安全复制到独占智能指针中
    auto asset = std::make_shared<ei::mimgs::ImageAsset>(assetId, img.cols, img.rows, fmt);
    size_t totalBytes = static_cast<size_t>(img.cols) * img.rows * img.channels();

    if (img.isContinuous())
    {
        std::memcpy(asset->data.get(), img.data, totalBytes);
    }
    else
    {
        size_t rowBytes = img.cols * img.channels();
        for (int r = 0; r < img.rows; ++r)
        {
            std::memcpy(asset->data.get() + r * rowBytes, img.ptr(r), rowBytes);
        }
    }

    return asset;
}

int main()
{
    try
    {
        ilog("=======================================================");
        ilog("        正在初始化客户端图像数据与流水线...            ");
        ilog("=======================================================");

        // 自定义你的本地图片路径（可放置在项目根目录下，用相对或绝对路径读取）
        std::string localImagePath = "/app/image.png"; 

        // 1. 替代原有的 mock 纯色内存分配，直接从本地磁盘加载真实图片
        ilog("[Client] 正在从磁盘载入图像: {}", localImagePath);
        auto realInputAsset = LoadLocalImage(localImagePath, "origin_canvas");
        ilog("[Client] 图像载入成功. 分辨率: {}x{}, 通道数: {}", 
             realInputAsset->width, realInputAsset->height, 
             (realInputAsset->format == ei::mimgs::PixelFormat::RGBA8888 ? 4 : 3));

        // 2. 构建输入黑板资产字典
        std::unordered_map<std::string, std::shared_ptr<ei::mimgs::ImageAsset>> inputMap;
        inputMap["origin_canvas"] = realInputAsset;

        // 3. 自由声明任务流水线参数
        ei::mimgs::PipelineParams pipelineConfig;
        pipelineConfig.inputKey = {"origin_canvas"}; 
        pipelineConfig.outputKey = "final_studio_artifact"; 

        // 步骤一：配置缩放算子 (示例：将原图强转缩放到 400x400)
        ei::mimgs::PipelineStep firstStep;
        firstStep.outputKey = "thumb_half_size";
        firstStep.params = ei::mimgs::ScaleStep{
            .inputKey = "origin_canvas",
            .tarWidth = 400,
            .tarHeight = 400
        };
        pipelineConfig.steps.push_back(firstStep);

        // 步骤二：配置旋转算子 (将缩放后的产物顺时针旋转 45 度)
        ei::mimgs::PipelineStep secondStep;
        secondStep.outputKey = "final_studio_artifact";
        secondStep.params = ei::mimgs::RotateStep{
            .inputKey = "thumb_half_size",
            .angle = 45.0f
        };
        pipelineConfig.steps.push_back(secondStep);

        // 4. 实例化 SDK 门面类并调用执行
        ei::mimgs::Images mimgsSdk;
        std::unordered_map<std::string, std::shared_ptr<ei::mimgs::ImageAsset>> outputMap;

        ilog("[Client] 启动 SDK 核心流水线引擎...");
        bool isExecuted = mimgsSdk.execute(pipelineConfig, inputMap, outputMap);

        // 5. 处理并检验输出成果
        if (isExecuted)
        {
            ilog("[Client] 核心流水线引擎执行成功！");

            auto it = outputMap.find("final_studio_artifact");
            if (it != outputMap.end() && it->second)
            {
                auto finalResult = it->second;
                ilog(">>> 成功提取最终图像资产信息 <<<");
                ilog(" 资产标识 (ID)  : {}", finalResult->id);
                ilog(" 最终分辨率宽高 : {} x {}", finalResult->width, finalResult->height);
                ilog(" 内存指针首地址 : {}", static_cast<void*>(finalResult->data.get()));
                
                // 提示：你也可以在这里利用 OpenCV 将 finalResult->data 写回本地磁盘查看效果
            }
            else
            {
                elog("[Client] 错误：在输出字典中未找到预期的最终成果键。");
            }
        }
        else
        {
            elog("[Client] 严重错误：SDK 流水线内部执行熔断返回失败。");
        }
    }
    catch (const std::exception &e)
    {
        elog("[Client] 捕获未达预期异常: {}", e.what());
    }

    return 0;
}

#elif defined(DoTest)
#include "mlog/Log.hpp"

#include <iostream>
#include <thread>
#include <sstream>
#include <vector>

void TaskT1(size_t id)
{
    auto tid_obj = std::this_thread::get_id();

    std::stringstream ss;
    ss << tid_obj;

    std::string tid = ss.str();

    for (size_t i = 0; i < 10; ++i)
    {
        ilog("id[{}] -- threadId[{}]:> {}", id, tid, i);
    }
}

int main()
{
    std::cout << "hello, world" << std::endl;

    ilog("INFO log printf");
    ilog("INFO Value: {}", "good boys");

    std::vector<std::thread> threads;
    for (size_t i = 0; i < 3; ++i)
    {
        threads.push_back(std::thread(TaskT1, i));
    }

    for (auto &t : threads)
    {
        if (t.joinable())
            t.join();
    }

    return 0;
}

#endif
