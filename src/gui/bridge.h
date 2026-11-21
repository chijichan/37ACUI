#pragma once

// ============================================================================
// 37AC 桥接层
//
// 37ACUI 只负责界面，模型信息 / 检查点 / 基模 / 数据集等操作都交给 37AC 自身
// 的 Python 服务实现（单一事实来源）。这里负责：
//   1. 把内嵌的 acui_bridge.py 落到程序目录（随 exe 一起分发，无需额外文件）
//   2. 生成命令行
//   3. 解析桥接脚本输出的行式协议
//
// 协议示例：
//   INFO
//   version=0.0.4
//   class_count=210
//   CKPT
//   name=0.0.4-20261121_153012-baseline
//   kind=baseline
//   ...
//   -
//   END
// ============================================================================

#include <string>
#include <vector>

namespace Bridge
{

struct ModelInfo
{
    bool ok = false;
    std::string error;
    std::string version;
    std::string trainedAt;
    std::string modelFile;
    std::string base;
    std::string bestAcc;
    std::string dataset;
    std::string imageSize;
    std::string batchSize;
    std::string sha;
    int classCount = 0;
    double sizeMb = 0.0;
    bool modelExists = false;
    int checkpointCount = 0;
};

struct Checkpoint
{
    std::string name;
    std::string kind;
    std::string accuracy;
    std::string createdAt;
    double sizeMb = 0.0;
    bool exists = true;
};

struct Info
{
    ModelInfo model;
    std::vector<Checkpoint> checkpoints;
};

// 把内嵌脚本写入 programDir/acui_bridge.py（内容一致时不重写），返回脚本路径
std::string ensureScript(const std::string &programDir);

// 生成 python 可执行参数：`"<abs path>" <args>`（含空格路径已带引号）
std::string command(const std::string &scriptPath, const std::string &args);

// 解析 `info` 输出
bool parseInfo(const std::string &output, Info &out);

// 解析动作命令输出（OK / ERR=xxx）
bool parseStatus(const std::string &output, std::string &detail, std::string &error);

} // namespace Bridge
