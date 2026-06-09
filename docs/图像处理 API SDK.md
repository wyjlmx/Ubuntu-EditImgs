为了实现企业级 C++ 项目的**物理编译解耦**与**接口稳定（ABI 稳定性）**，我们设计了一套符合工业标准的目录与文件结构。

在本设计中，所有核心功能拆分为 3 大核心层次：
1. **门面层 (Facade/SDK Layer)**：外部客户唯一可见的头文件。采用 **Pimpl（编译防火墙）** 模式，实现物理编译隔离与二进制接口（ABI）保护。
2. **调度层 (Orchestration/Scheduler Layer)**：`.so` 内部核心。解析高层业务配置，将其拆分为一系列算子步骤，管理运行时上下文。
3. **业务/算子层 (Business/Operator Layer)**：`.so` 内部底层。单一职责，实现具体的缩放、旋转、拼图等核心算法。

---

### 📂 物理项目结构概览

```text
.
├── include/                              # SDK 外部头文件（用户可见）
│   └── image_processor_sdk.h             # 门面类声明 (Pimpl 模式)
├── src/                                  # 内部实现代码（用户不可见）
│   ├── common/
│   │   └── internal_common.h             # 内部公共接口、上下文和基础结构
│   ├── operators/
│   │   ├── internal_operators.h          # 算子类声明
│   │   └── internal_operators.cpp        # 算子类具体实现
│   ├── scheduler/
│   │   ├── pipeline_scheduler.h          # 流水线调度器声明
│   │   └── pipeline_scheduler.cpp        # 流水线调度器实现
│   └── image_processor_sdk.cpp           # 门面类实现 (Impl 具体实现)
└── main.cpp                              # 外部业务调用示例 (Client)
```

---

### Code Block 1: 门面层 - SDK 外部头文件 (用户可见)
*文件名：`include/image_processor_sdk.h`*

```cpp
#pragma once

#include <string>
#include <memory>

namespace Enterprise::ImageProc {

// 外部业务配置结构体（完全不暴露任何内部指针或底层类型）
struct TaskConfig {
    std::string input_path_a;
    std::string input_path_b;
    std::string output_path;
    int scale_width{400};
    int scale_height{400};
    float rotate_angle{90.0f};
    int stitch_gap{20};
};

// 外部 SDK 门面类
class ImageProcessorSDK {
public:
    ImageProcessorSDK();
    
    // 析构函数必须在 .cpp 中定义以使用 default，避免 std::unique_ptr 遇到前置声明的不完整类型报错
    ~ImageProcessorSDK();

    // 禁用拷贝语义，防止外部调用时意外复制庞大的底层状态
    ImageProcessorSDK(const ImageProcessorSDK&) = delete;
    ImageProcessorSDK& operator=(const ImageProcessorSDK&) = delete;

    // 支持移动语义
    ImageProcessorSDK(ImageProcessorSDK&&) noexcept;
    ImageProcessorSDK& operator=(ImageProcessorSDK&&) noexcept;

    // 对外提供的统一高层 API
    bool ProcessImage(const TaskConfig& config);

private:
    class Impl;                     // 前置声明：具体实现类
    std::unique_ptr<Impl> pimpl_;   // 唯一的成员变量：指向实现的指针
};

} // namespace Enterprise::ImageProc
```

---

### Code Block 2: 内部公共模块 - 基础结构、上下文与接口 (内部共享)
*文件名：`src/common/internal_common.h`*

```cpp
#pragma once

#include <string>
#include <memory>
#include <unordered_map>
#include <stdexcept>

namespace Enterprise::ImageProc::Internal {

// 企业级自定义内部异常
class ImageProcessingException : public std::runtime_error {
public:
    explicit ImageProcessingException(const std::string& msg) 
        : std::runtime_error("[Internal Error] " + msg) {}
};

enum class PixelFormat { RGBA8888, RGB888 };

// 内部图像基础资源：管理生命周期和独占内存
struct ImageAsset {
    std::string id;
    int width{0};
    int height{0};
    PixelFormat format{PixelFormat::RGBA8888};
    std::unique_ptr<uint8_t[]> data{nullptr};

    ImageAsset(std::string_view asset_id, int w, int h, PixelFormat fmt)
        : id(asset_id), width(w), height(h), format(fmt) {
        size_t size = static_cast<size_t>(w) * h * (fmt == PixelFormat::RGBA8888 ? 4 : 3);
        data = std::make_unique<uint8_t[]>(size);
    }
    
    // 禁用拷贝以实现零拷贝安全传输
    ImageAsset(const ImageAsset&) = delete;
    ImageAsset& operator=(const ImageAsset&) = delete;
    ImageAsset(ImageAsset&&) noexcept = default;
    ImageAsset& operator=(ImageAsset&&) noexcept = default;
};

// 内部共享上下文（黑板模式）：每轮业务请求对应一个独立的 Context 实例
class ImageContext {
private:
    std::unordered_map<std::string, std::shared_ptr<ImageAsset>> assets_;

public:
    void SetAsset(std::shared_ptr<ImageAsset> asset) {
        if (!asset || asset->id.empty()) {
            throw ImageProcessingException("Null or invalid asset.");
        }
        assets_[asset->id] = std::move(asset);
    }

    std::shared_ptr<ImageAsset> GetAsset(const std::string& id) const {
        auto it = assets_.find(id);
        if (it != assets_.end()) return it->second;
        return nullptr;
    }
};

// 所有算子参数的基类
struct OperatorParams {
    std::string output_key;
    virtual ~OperatorParams() = default;
    virtual void Validate() const {
        if (output_key.empty()) throw ImageProcessingException("output_key is empty");
    }
};

// 统一算子契约接口
class IImageOperator {
public:
    virtual ~IImageOperator() = default;
    virtual std::string_view GetName() const noexcept = 0;
    virtual void Execute(ImageContext& context, const OperatorParams& params) = 0;

protected:
    template <typename T>
    const T& CastParams(const OperatorParams& params) const {
        const auto* derived = dynamic_cast<const T*>(&params);
        if (!derived) {
            throw ImageProcessingException(std::string(GetName()) + ": Parameter mismatch.");
        }
        return *derived;
    }
};

} // namespace Enterprise::ImageProc::Internal
```

---

### Code Block 3: 业务/算子层 - 声明与具体算法实现 (内部组件)
*文件名：`src/operators/internal_operators.h`*

```cpp
#pragma once

#include "common/internal_common.h"
#include <vector>

namespace Enterprise::ImageProc::Internal {

// 缩放算子
struct ScaleParams : public OperatorParams {
    std::string input_key;
    int target_width{0};
    int target_height{0};

    void Validate() const override;
};

class ImageScaleOperator : public IImageOperator {
public:
    std::string_view GetName() const noexcept override { return "scale"; }
    void Execute(ImageContext& context, const OperatorParams& params) override;
};

// 旋转算子
struct RotateParams : public OperatorParams {
    std::string input_key;
    float angle{0.0f};

    void Validate() const override;
};

class ImageRotateOperator : public IImageOperator {
public:
    std::string_view GetName() const noexcept override { return "rotate"; }
    void Execute(ImageContext& context, const OperatorParams& params) override;
};

// 拼图算子
struct StitchParams : public OperatorParams {
    std::vector<std::string> input_keys;
    int gap{0};

    void Validate() const override;
};

class ImageStitchOperator : public IImageOperator {
public:
    std::string_view GetName() const noexcept override { return "stitch"; }
    void Execute(ImageContext& context, const OperatorParams& params) override;
};

} // namespace Enterprise::ImageProc::Internal
```

*文件名：`src/operators/internal_operators.cpp`*

```cpp
#include "operators/internal_operators.h"
#include <iostream>
#include <algorithm>

namespace Enterprise::ImageProc::Internal {

// --- 缩放算子实现 ---
void ScaleParams::Validate() const {
    OperatorParams::Validate();
    if (input_key.empty()) throw ImageProcessingException("Scale: input_key empty.");
    if (target_width <= 0 || target_height <= 0) throw ImageProcessingException("Scale: invalid size.");
}

void ImageScaleOperator::Execute(ImageContext& context, const OperatorParams& params) {
    const auto& scale_params = CastParams<ScaleParams>(params);
    scale_params.Validate();

    auto source = context.GetAsset(scale_params.input_key);
    if (!source) throw ImageProcessingException("Scale: " + scale_params.input_key + " not found.");

    std::cout << "[Operator::Scale] Processing " << scale_params.input_key 
              << " -> " << scale_params.target_width << "x" << scale_params.target_height << "\n";

    auto output = std::make_shared<ImageAsset>(scale_params.output_key, scale_params.target_width, scale_params.target_height, source->format);
    context.SetAsset(std::move(output));
}

// --- 旋转算子实现 ---
void RotateParams::Validate() const {
    OperatorParams::Validate();
    if (input_key.empty()) throw ImageProcessingException("Rotate: input_key empty.");
}

void ImageRotateOperator::Execute(ImageContext& context, const OperatorParams& params) {
    const auto& rotate_params = CastParams<RotateParams>(params);
    rotate_params.Validate();

    auto source = context.GetAsset(rotate_params.input_key);
    if (!source) throw ImageProcessingException("Rotate: " + rotate_params.input_key + " not found.");

    std::cout << "[Operator::Rotate] Rotating " << rotate_params.input_key << " by " << rotate_params.angle << " deg.\n";

    auto output = std::make_shared<ImageAsset>(rotate_params.output_key, source->height, source->width, source->format); // 模拟 90 度旋转
    context.SetAsset(std::move(output));
}

// --- 拼图算子实现 ---
void StitchParams::Validate() const {
    OperatorParams::Validate();
    if (input_keys.empty()) throw ImageProcessingException("Stitch: input_keys empty.");
}

void ImageStitchOperator::Execute(ImageContext& context, const OperatorParams& params) {
    const auto& stitch_params = CastParams<StitchParams>(params);
    stitch_params.Validate();

    std::vector<std::shared_ptr<ImageAsset>> sources;
    int total_width = 0;
    int max_height = 0;

    for (const auto& key : stitch_params.input_keys) {
        auto asset = context.GetAsset(key);
        if (!asset) throw ImageProcessingException("Stitch: Input " + key + " not found.");
        sources.push_back(asset);
        total_width += asset->width;
        max_height = std::max(max_height, asset->height);
    }

    if (sources.size() > 1) {
        total_width += static_cast<int>(sources.size() - 1) * stitch_params.gap;
    }

    std::cout << "[Operator::Stitch] Stitching " << sources.size() << " assets -> output " << total_width << "x" << max_height << "\n";

    auto output = std::make_shared<ImageAsset>(stitch_params.output_key, total_width, max_height, sources[0]->format);
    context.SetAsset(std::move(output));
}

} // namespace Enterprise::ImageProc::Internal
```

---

### Code Block 4: 调度层 - 流水线与调度管理 (内部调度)
*文件名：`src/scheduler/pipeline_scheduler.h`*

```cpp
#pragma once

#include "common/internal_common.h"
#include <vector>

namespace Enterprise::ImageProc::Internal {

class PipelineScheduler {
public:
    struct Step {
        std::string operator_name;
        std::shared_ptr<OperatorParams> params;
    };

    PipelineScheduler();

    // 禁用拷贝
    PipelineScheduler(const PipelineScheduler&) = delete;
    PipelineScheduler& operator=(const PipelineScheduler&) = delete;

    // 调度执行
    void ExecutePipeline(ImageContext& context, const std::vector<Step>& steps) const;

private:
    std::unordered_map<std::string, std::unique_ptr<IImageOperator>> operators_;
};

} // namespace Enterprise::ImageProc::Internal
```

*文件名：`src/scheduler/pipeline_scheduler.cpp`*

```cpp
#include "scheduler/pipeline_scheduler.h"
#include "operators/internal_operators.h"
#include <iostream>

namespace Enterprise::ImageProc::Internal {

PipelineScheduler::PipelineScheduler() {
    // 内部注册表中注册底层算子（这块在内部是统一高内聚的）
    operators_["scale"] = std::make_unique<ImageScaleOperator>();
    operators_["rotate"] = std::make_unique<ImageRotateOperator>();
    operators_["stitch"] = std::make_unique<ImageStitchOperator>();
}

void PipelineScheduler::ExecutePipeline(ImageContext& context, const std::vector<Step>& steps) const {
    std::cout << "\n[Scheduler] Running Task Pipeline...\n";
    for (const auto& step : steps) {
        auto it = operators_.find(step.operator_name);
        if (it == operators_.end()) {
            throw ImageProcessingException("Scheduler: operator '" + step.operator_name + "' not supported.");
        }
        
        // 调度核心调用：统一的多态函数
        it->second->Execute(context, *(step.params));
    }
    std::cout << "[Scheduler] Pipeline Finished.\n\n";
}

} // namespace Enterprise::ImageProc::Internal
```

---

### Code Block 5: 门面实现层 - 用户不可见的 Pimpl 实现
*文件名：`src/image_processor_sdk.cpp`*

```cpp
#include "image_processor_sdk.h"
#include "common/internal_common.h"
#include "operators/internal_operators.h"
#include "scheduler/pipeline_scheduler.h"
#include <iostream>

namespace Enterprise::ImageProc {

// 实体的具体实现（Impl 类）放置在 .cpp 中
class ImageProcessorSDK::Impl {
private:
    Internal::PipelineScheduler scheduler_;

public:
    Impl() = default;

    bool ProcessImage(const TaskConfig& config) {
        try {
            // 1. 创建本批次任务隔离的运行时上下文，保证线程安全性
            Internal::ImageContext context;

            // 2. 模拟从外部加载图片资源到 Context 容器中
            std::cout << "[SDK] Mock Loading: " << config.input_path_a << "\n";
            std::cout << "[SDK] Mock Loading: " << config.input_path_b << "\n";
            
            auto asset_a = std::make_shared<Internal::ImageAsset>("image_a", 1920, 1080, Internal::PixelFormat::RGBA8888);
            auto asset_b = std::make_shared<Internal::ImageAsset>("image_b", 800, 800, Internal::PixelFormat::RGBA8888);
            
            context.SetAsset(std::move(asset_a));
            context.SetAsset(std::move(asset_b));

            // 3. 将外部简易配置（TaskConfig）翻译成调度层认识的细粒度算子参数列表
            std::vector<Internal::PipelineScheduler::Step> steps;

            // Step 1: 缩放 Image A
            auto scale_step = std::make_shared<Internal::ScaleParams>();
            scale_step->input_key = "image_a";
            scale_step->output_key = "image_a_scaled";
            scale_step->target_width = config.scale_width;
            scale_step->target_height = config.scale_height;
            steps.push_back({ "scale", scale_step });

            // Step 2: 旋转 Image B
            auto rotate_step = std::make_shared<Internal::RotateParams>();
            rotate_step->input_key = "image_b";
            rotate_step->output_key = "image_b_rotated";
            rotate_step->angle = config.rotate_angle;
            steps.push_back({ "rotate", rotate_step });

            // Step 3: 拼接已缩放 A 和已旋转 B
            auto stitch_step = std::make_shared<Internal::StitchParams>();
            stitch_step->input_keys = { "image_a_scaled", "image_b_rotated" };
            stitch_step->output_key = "image_final";
            stitch_step->gap = config.stitch_gap;
            steps.push_back({ "stitch", stitch_step });

            // 4. 交给内部调度层进行流水线操作
            scheduler_.ExecutePipeline(context, steps);

            // 5. 模拟写出结果
            auto final_asset = context.GetAsset("image_final");
            if (final_asset) {
                std::cout << "[SDK] Mock Saving result to: " << config.output_path 
                          << " (Resolution: " << final_asset->width << "x" << final_asset->height << ")\n";
            }
            return true;

        } catch (const std::exception& ex) {
            std::cerr << "[SDK Critical failure]: " << ex.what() << "\n";
            return false;
        }
    }
};

// 门面层成员函数的具体实现（在此调用 Impl，保证对外部用户隐藏这些依赖细节）
ImageProcessorSDK::ImageProcessorSDK() : pimpl_(std::make_unique<Impl>()) {}
ImageProcessorSDK::~ImageProcessorSDK() = default;

ImageProcessorSDK::ImageProcessorSDK(ImageProcessorSDK&&) noexcept = default;
ImageProcessorSDK& ImageProcessorSDK::operator=(ImageProcessorSDK&&) noexcept = default;

bool ImageProcessorSDK::ProcessImage(const TaskConfig& config) {
    return pimpl_->ProcessImage(config);
}

} // namespace Enterprise::ImageProc
```

---

### Code Block 6: 外部客户端调用层 (Client)
*文件名：`main.cpp`*

```cpp
// 外部调用者只需要 #include 我们唯一的门面 SDK 头文件即可
// 无需知道内部的 IImageOperator、ImageContext、内部异常、具体算子等任何底层逻辑
#include "image_processor_sdk.h"
#include <iostream>

int main() {
    using namespace Enterprise::ImageProc;

    std::cout << "[Client] Starting Business Logic...\n";

    // 1. 初始化门面类实例
    ImageProcessorSDK sdk;

    // 2. 准备简单的业务配置参数
    TaskConfig config;
    config.input_path_a = "path/to/my_avatar.png";
    config.input_path_b = "path/to/brand_logo.jpg";
    config.output_path  = "path/to/final_stitching.jpg";
    config.scale_width  = 300;
    config.scale_height = 300;
    config.rotate_angle = 90.0f;
    config.stitch_gap   = 15;

    // 3. 直接调用门面层 API，极度干净利落
    bool success = sdk.ProcessImage(config);

    if (success) {
        std::cout << "[Client] Image processed and output saved.\n";
    } else {
        std::cerr << "[Client] Image processing failed!\n";
    }

    return 0;
}
```

---

### 📝 架构设计解读

1. **绝对的物理隔离 (SDK Level)**：
   在打包分发 `.so` 时，您只需要提供 `image_processor_sdk.h`。客户的 `main.cpp` 绝不会引入 `internal_common.h` 里的容器、智能指针或其它算法头文件。这大幅缩短了客户工程的编译周期，并确保了商业级代码实现的非公开安全性。
2. **多线程并发安全性**：
   在 `ImageProcessorSDK::Impl::ProcessImage` 函数执行时，`ImageContext` 是在函数栈上作为局部变量创建并传递的，这意味着不同线程同时调用这个 SDK 实例处理不同图片时，**互相之间不存在任何共享状态或数据竞争（Data Race）**，天然支持多线程。
3. **接口扩展性（算子闭包）**：
   如果您后期希望在调度中增加一个 `ImageCropOperator`，您只需要在 `src/operators/` 下写一个新的算子，并注册到 `PipelineScheduler` 中。由于修改均发生在 `.so` 内部，外部客户端完全不需要重新编译，更不需要对代码做任何哪怕一个字节的修改。