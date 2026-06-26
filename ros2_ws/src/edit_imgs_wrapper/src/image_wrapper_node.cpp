#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/image.hpp>
#include <cv_bridge/cv_bridge.hpp>
#include <opencv2/opencv.hpp>

// 引入你的核心 SDK 门面
#include <mimgs/Mimgs.hpp>
#include <mimgs/types/EImages.hpp>
#include <mimgs/interfaces/IImages.hpp>

class ImageWrapperNode : public rclcpp::Node
{
public:
    ImageWrapperNode() : Node("image_wrapper_node")
    {
        // 1. 初始化你的企业级 SDK (主厨)
        _images_sdk = std::make_unique<ei::mimgs::Images>();

        // 2. 声明发布：准备好在 "processed_image" 频道上菜
        _publisher = this->create_publisher<sensor_msgs::msg::Image>("processed_image", 10);

        // 3. 声明订阅：听取 "raw_image" 频道的订单
        _subscriber = this->create_subscription<sensor_msgs::msg::Image>(
            "raw_image", 10,
            std::bind(&ImageWrapperNode::image_callback, this, std::placeholders::_1));

        RCLCPP_INFO(this->get_logger(), "翻译官节点已启动，正在监听 /raw_image 话题...");
    }

private:
    std::unique_ptr<ei::mimgs::IImages> _images_sdk;
    rclcpp::Publisher<sensor_msgs::msg::Image>::SharedPtr _publisher;
    rclcpp::Subscription<sensor_msgs::msg::Image>::SharedPtr _subscriber;

    // 当收到图像时的回调函数 (核心数据流转)
    void image_callback(const sensor_msgs::msg::Image::SharedPtr msg)
    {
        try
        {
            // ==========================================
            // [阶段 A: 翻译官接单] ROS msg -> cv::Mat -> SDK ImageAsset
            // ==========================================
            // 使用 cv_bridge 将 ROS 消息转为 OpenCV 格式 (假设使用 BGR8)
            cv_bridge::CvImagePtr cv_ptr = cv_bridge::toCvCopy(msg, sensor_msgs::image_encodings::BGR8);

            // 将 OpenCV 的数据封装成你的 SDK 认识的 ImageAsset
            auto input_asset = std::make_shared<ei::mimgs::ImageAsset>(
                "input_1",
                cv_ptr->image.cols,
                cv_ptr->image.rows,
                ei::mimgs::PixelFormat::RGB888 // 假设映射为 RGB888 或保持通道一致
            );
            // 将像素数据拷贝进你的 Asset 内存 (注意内存大小)
            size_t data_size = cv_ptr->image.total() * cv_ptr->image.elemSize();
            std::memcpy(input_asset->data.get(), cv_ptr->image.data, data_size);

            // ==========================================
            // [阶段 B: 主厨做菜] 调用纯 C++ SDK
            // ==========================================
            std::unordered_map<std::string, std::shared_ptr<ei::mimgs::ImageAsset>> inputs;
            inputs["input_1"] = input_asset;

            std::unordered_map<std::string, std::shared_ptr<ei::mimgs::ImageAsset>> outputs;

            ei::mimgs::PipelineParams params;
            params.inputKey.push_back("input_1");

            // 【修改 1】：告诉 SDK，最终需要的产物就是原始输入
            params.outputKey = "input_1";

            bool success = _images_sdk->execute(params, inputs, outputs);

            // 【修改 2】：检查的键值改为 input_1
            if (!success || outputs.find("input_1") == outputs.end())
            {
                RCLCPP_ERROR(this->get_logger(), "SDK 处理图像失败！");
                return;
            }

            // ==========================================
            // [阶段 C: 服务员上菜] SDK ImageAsset -> cv::Mat -> ROS msg
            // ==========================================
            auto result_asset = outputs["input_1"];

            // 包装回 cv::Mat
            cv::Mat result_mat(
                result_asset->height,
                result_asset->width,
                CV_8UC3, // 假设 3 通道
                result_asset->data.get());

            // 通过 cv_bridge 打包成 ROS 2 消息并发布
            std_msgs::msg::Header header;
            header.stamp = this->now();
            auto result_msg = cv_bridge::CvImage(header, sensor_msgs::image_encodings::BGR8, result_mat).toImageMsg();

            _publisher->publish(*result_msg);
            RCLCPP_INFO(this->get_logger(), "成功处理并发布了一张图片！(大小: %dx%d)", result_asset->width, result_asset->height);
        }
        catch (const cv_bridge::Exception &e)
        {
            RCLCPP_ERROR(this->get_logger(), "cv_bridge 转换失败: %s", e.what());
        }
        catch (const std::exception &e)
        {
            RCLCPP_ERROR(this->get_logger(), "SDK 发生异常: %s", e.what());
        }
    }
};

// 程序的入口 (这就解决了刚才报错的 undefined reference to 'main')
int main(int argc, char **argv)
{
    rclcpp::init(argc, argv);
    rclcpp::spin(std::make_shared<ImageWrapperNode>());
    rclcpp::shutdown();
    return 0;
}