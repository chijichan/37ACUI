#include "bridge.h"

#include "imgui.h" // 仅为复用工程内统一的字符串工具风格（不参与渲染）

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>

namespace Bridge
{

// ===========================================================================
// 内嵌桥接脚本
// 只在需要时写入程序目录；与 37AC 的 CLI 模块保持单一事实来源。
// ===========================================================================
static const char *kBridgeScript = R"PY(#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""37ACUI bridge — 桌面端调用 37AC CLI 能力的稳定入口。

由 37ACUI 自动写入程序目录，以 ``python acui_bridge.py <command> [args]`` 运行，
工作目录固定为 37AC 的 src/cli。输出行式协议，便于 C++ 侧解析：

    INFO                模型信息块开始
    key=value           字段
    CKPT                检查点列表开始
    name=...            每个检查点一组字段，组间以 "-" 分隔
    END

动作类命令输出 ``OK`` / ``OK=细节``；失败输出 ``ERR=原因`` 并以非 0 退出。
"""

import os
import sys

VERSION = "1"


def _prepare():
    cwd = os.getcwd()
    if os.path.isfile(os.path.join(cwd, "main.py")):
        sys.path.insert(0, cwd)
    try:
        from config.log_config import init_logging
        init_logging()
    except Exception:
        pass


def _flat(value):
    if value is None:
        return ""
    if isinstance(value, (dict, list, tuple)):
        import json
        value = json.dumps(value, ensure_ascii=False)
    return str(value).replace("\r", " ").replace("\n", " ").strip()


def _ok(detail=""):
    print("OK" + ("=" + _flat(detail) if detail else ""))
    return 0


def _fail(message="未知错误", code=1):
    print("ERR=" + _flat(message))
    return code


# ---------------------------------------------------------------- 模型信息
def cmd_info():
    from services.checkpoint_service import current_info, list_checkpoints

    info = current_info()
    print("INFO")
    for key in ("version", "trained_at", "class_count", "model_file", "model_exists",
                "model_size_mb", "model_sha256", "base", "best_val_acc", "dataset",
                "image_size", "batch_size", "checkpoints"):
        print("%s=%s" % (key, _flat(info.get(key))))
    print("CKPT")
    for item in list_checkpoints():
        print("name=%s" % _flat(item.get("name")))
        print("kind=%s" % _flat(item.get("kind")))
        print("size_mb=%s" % _flat(item.get("size_mb")))
        print("accuracy=%s" % _flat(item.get("accuracy")))
        print("created_at=%s" % _flat(item.get("created_at")))
        print("exists=%s" % ("1" if item.get("exists") else "0"))
        print("-")
    print("END")
    return 0


# ---------------------------------------------------------------- 检查点
def cmd_ckpt_restore(name):
    from services.checkpoint_service import restore_checkpoint
    result = restore_checkpoint(name)
    if not result:
        return _fail("回滚失败: %s" % name)
    return _ok(result.get("backup") or "")


def cmd_ckpt_delete(name):
    from services.checkpoint_service import delete_checkpoint
    return _ok() if delete_checkpoint(name) else _fail("删除失败: %s" % name)


def cmd_ckpt_prune(keep="10", max_mb="2000"):
    from services.checkpoint_service import prune
    result = prune(keep=int(keep), max_total_mb=float(max_mb))
    detail = "removed=%d kept=%d total=%.1fMB" % (
        len(result.get("removed") or []), result.get("kept") or 0, result.get("total_mb") or 0.0)
    return _ok(detail)


# ---------------------------------------------------------------- 训练基模 / HF
def cmd_base_download(repo):
    from services.hf_download import download_base
    info = download_base(repo)
    return _ok(info.get("id") or repo)


def cmd_hf_login(token=None):
    from services.hf_download import login
    token = (token or os.getenv("HF_TOKEN") or "").strip()
    if not token:
        return _fail("缺少 HF Token")
    info = login(token)
    return _ok(info.get("name") or "")


def cmd_publish(repo, token=None):
    from config.base import MODEL_PATH, CLASSES_JSON_PATH, MODEL_INFO_PATH
    from services.hf_download import publish_model

    missing = [str(p) for p in (MODEL_PATH, CLASSES_JSON_PATH, MODEL_INFO_PATH) if not p.exists()]
    if missing:
        return _fail("模型包不完整: " + ", ".join(missing))
    info = publish_model(repo_id=repo, model_path=MODEL_PATH, classes_path=CLASSES_JSON_PATH,
                         config_path=MODEL_INFO_PATH,
                         token=(token or os.getenv("HF_TOKEN") or None), private=True)
    if info.get("weights_url"):
        print("URL=" + _flat(info.get("weights_url")))
    return _ok(repo)


# ---------------------------------------------------------------- 数据集
def cmd_dataset_verify():
    from services.menu_service import verify_images_function
    verify_images_function()
    return _ok()


def cmd_dataset_crop():
    from config.base import DATASET_DIR, CROPPED_DATASET_DIR, MAX_IMAGES_PER_ROLE
    from detection.yolo_detector import crop_dataset
    from utils.concurrency import CancelToken

    result = crop_dataset(str(DATASET_DIR), str(CROPPED_DATASET_DIR),
                          max_images_per_role=MAX_IMAGES_PER_ROLE,
                          cancel_event=CancelToken())
    return _ok("processed=%d skipped=%d failed=%d" % (
        result.get("processed", 0), result.get("skipped", 0), result.get("failed", 0)))


def cmd_dataset_compress():
    from config.base import (CROPPED_DATASET_DIR, DATASET_COMPRESS_SIZE,
                             DATASET_COMPRESS_QUALITY, DATASET_COMPRESS_WORKERS)
    from utils.image_utils import compress_dataset_images
    from utils.concurrency import CancelToken

    if DATASET_COMPRESS_SIZE <= 0:
        return _fail("数据集压缩已禁用（DATASET_COMPRESS_SIZE=0），请在 .env 中配置")
    result = compress_dataset_images(str(CROPPED_DATASET_DIR),
                                     max_size=DATASET_COMPRESS_SIZE,
                                     quality=DATASET_COMPRESS_QUALITY,
                                     workers=DATASET_COMPRESS_WORKERS,
                                     cancel_token=CancelToken())
    return _ok("total=%d compressed=%d skipped=%d failed=%d" % (
        result.get("total", 0), result.get("compressed", 0),
        result.get("skipped", 0), result.get("failed", 0)))


def cmd_dataset_jpeg():
    from config.base import CROPPED_DATASET_DIR, DATASET_COMPRESS_QUALITY, DATASET_COMPRESS_WORKERS
    from utils.image_utils import convert_dataset_to_jpeg
    from utils.concurrency import CancelToken

    result = convert_dataset_to_jpeg(str(CROPPED_DATASET_DIR),
                                     quality=DATASET_COMPRESS_QUALITY,
                                     workers=DATASET_COMPRESS_WORKERS,
                                     cancel_token=CancelToken())
    return _ok("converted=%d skipped=%d failed=%d" % (
        result.get("converted", 0), result.get("skipped", 0), result.get("failed", 0)))


def cmd_dataset_info():
    from config.base import DATASET_DIR, CROPPED_DATASET_DIR
    print("INFO")
    print("raw=%s" % _flat(DATASET_DIR))
    print("raw_exists=%s" % ("1" if os.path.isdir(str(DATASET_DIR)) else "0"))
    print("cropped=%s" % _flat(CROPPED_DATASET_DIR))
    print("cropped_exists=%s" % ("1" if os.path.isdir(str(CROPPED_DATASET_DIR)) else "0"))
    print("END")
    return 0


COMMANDS = {
    "info": cmd_info,
    "ckpt-restore": cmd_ckpt_restore,
    "ckpt-delete": cmd_ckpt_delete,
    "ckpt-prune": cmd_ckpt_prune,
    "base-download": cmd_base_download,
    "hf-login": cmd_hf_login,
    "publish": cmd_publish,
    "dataset-verify": cmd_dataset_verify,
    "dataset-crop": cmd_dataset_crop,
    "dataset-compress": cmd_dataset_compress,
    "dataset-jpeg": cmd_dataset_jpeg,
    "dataset-info": cmd_dataset_info,
}


def main():
    _prepare()
    if len(sys.argv) < 2:
        return _fail("缺少命令；可用: " + ", ".join(sorted(COMMANDS)))
    name = sys.argv[1]
    handler = COMMANDS.get(name)
    if not handler:
        return _fail("未知命令: " + name)
    try:
        return handler(*sys.argv[2:])
    except Exception as exc:  # noqa: BLE001 - 桥接层需要把任何异常转成可读文案
        import traceback
        traceback.print_exc()
        return _fail("%s: %s" % (type(exc).__name__, exc))


if __name__ == "__main__":
    sys.exit(main())
)PY";

// ===========================================================================
// 工具
// ===========================================================================
namespace
{

bool readWholeFile(const std::string &path, std::string &out)
{
    std::ifstream f(path.c_str(), std::ios::binary);
    if (!f)
        return false;
    std::ostringstream ss;
    ss << f.rdbuf();
    out = ss.str();
    return true;
}

std::string joinPath(const std::string &dir, const char *name)
{
    std::string d = dir;
    while (!d.empty() && (d.back() == '/' || d.back() == '\\'))
        d.pop_back();
    return d + "/" + name;
}

bool startsWith(const std::string &s, const char *prefix)
{
    size_t n = strlen(prefix);
    return s.size() >= n && s.compare(0, n, prefix) == 0;
}

std::string trimLine(const std::string &s)
{
    size_t b = 0, e = s.size();
    while (b < e && (s[b] == ' ' || s[b] == '\t'))
        b++;
    while (e > b && (s[e - 1] == '\r' || s[e - 1] == '\n' || s[e - 1] == ' ' || s[e - 1] == '\t'))
        e--;
    return s.substr(b, e - b);
}

} // namespace

// ===========================================================================
// 脚本落地
// ===========================================================================
std::string ensureScript(const std::string &programDir)
{
    const std::string path = joinPath(programDir, "acui_bridge.py");
    std::string existing;
    if (readWholeFile(path, existing) && existing == std::string(kBridgeScript))
        return path;

    std::ofstream f(path.c_str(), std::ios::binary | std::ios::trunc);
    if (f)
    {
        f.write(kBridgeScript, (std::streamsize)strlen(kBridgeScript));
        f.close();
    }
    return path;
}

std::string command(const std::string &scriptPath, const std::string &args)
{
    std::string cmd = "\"" + scriptPath + "\"";
    if (!args.empty())
        cmd += " " + args;
    return cmd;
}

// ===========================================================================
// 协议解析
// ===========================================================================
bool parseInfo(const std::string &output, Info &out)
{
    out = Info();
    bool sawInfo = false;

    enum Stage
    {
        None,
        InInfo,
        InCkpt
    } stage = None;

    Checkpoint cur;
    bool haveCur = false;
    auto flushCur = [&]() {
        if (haveCur && !cur.name.empty())
            out.checkpoints.push_back(cur);
        cur = Checkpoint();
        haveCur = false;
    };

    std::istringstream ss(output);
    std::string raw;
    while (std::getline(ss, raw))
    {
        const std::string line = trimLine(raw);
        if (line.empty())
            continue;
        if (line == "INFO")
        {
            stage = InInfo;
            sawInfo = true;
            out.model.ok = true;
            continue;
        }
        if (line == "CKPT")
        {
            stage = InCkpt;
            continue;
        }
        if (line == "END")
            break;
        if (startsWith(line, "ERR="))
        {
            out.model.ok = false;
            out.model.error = line.substr(4);
            return false;
        }
        if (stage == InInfo)
        {
            const size_t eq = line.find('=');
            if (eq == std::string::npos)
                continue;
            const std::string key = line.substr(0, eq);
            const std::string value = line.substr(eq + 1);
            ModelInfo &m = out.model;
            if (key == "version")
                m.version = value;
            else if (key == "trained_at")
                m.trainedAt = value;
            else if (key == "model_file")
                m.modelFile = value;
            else if (key == "base")
                m.base = value;
            else if (key == "best_val_acc")
                m.bestAcc = value;
            else if (key == "dataset")
                m.dataset = value;
            else if (key == "image_size")
                m.imageSize = value;
            else if (key == "batch_size")
                m.batchSize = value;
            else if (key == "model_sha256")
                m.sha = value;
            else if (key == "class_count")
                m.classCount = value.empty() ? 0 : atoi(value.c_str());
            else if (key == "model_size_mb")
                m.sizeMb = value.empty() ? 0.0 : atof(value.c_str());
            else if (key == "model_exists")
                m.modelExists = (value == "1" || value == "True" || value == "true");
            else if (key == "checkpoints")
                m.checkpointCount = value.empty() ? 0 : atoi(value.c_str());
        }
        else if (stage == InCkpt)
        {
            if (line == "-")
            {
                flushCur();
                continue;
            }
            const size_t eq = line.find('=');
            if (eq == std::string::npos)
                continue;
            const std::string key = line.substr(0, eq);
            const std::string value = line.substr(eq + 1);
            if (key == "name")
            {
                flushCur();
                cur.name = value;
                haveCur = true;
            }
            else if (key == "kind")
                cur.kind = value;
            else if (key == "accuracy")
                cur.accuracy = value;
            else if (key == "created_at")
                cur.createdAt = value;
            else if (key == "size_mb")
                cur.sizeMb = value.empty() ? 0.0 : atof(value.c_str());
            else if (key == "exists")
                cur.exists = (value == "1" || value == "True" || value == "true");
        }
    }
    flushCur();
    return sawInfo;
}

bool parseStatus(const std::string &output, std::string &detail, std::string &error)
{
    detail.clear();
    error.clear();
    bool ok = false;

    std::istringstream ss(output);
    std::string raw;
    while (std::getline(ss, raw))
    {
        const std::string line = trimLine(raw);
        if (startsWith(line, "ERR="))
        {
            error = line.substr(4);
            return false;
        }
        if (line == "OK")
            ok = true;
        else if (startsWith(line, "OK="))
        {
            ok = true;
            detail = line.substr(3);
        }
    }
    if (!ok && error.empty())
        error = "桥接脚本未返回结果";
    return ok && error.empty();
}

} // namespace Bridge
