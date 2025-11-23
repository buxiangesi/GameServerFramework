● 实战演练：分支管理测试

  好的！我们现在就实操一遍，让你熟悉整个流程。

---
  步骤1：创建测试分支

  操作：
  1. 进入项目目录：D:\VS\learning_linux_11_6\learning_linux_11_6
  2. 在空白处右键
  3. 选择 TortoiseGit → Create Branch（创建分支）
  4. 填写信息：
     ┌─────────────────────────────────────┐
     │ Branch name: test-version-control   │
     │                                     │
     │ ☑ Switch to new branch              │ ← 勾选这个
     │                                     │
     │ Base On:                            │
     │ ⦿ Branch: [main ▼]                 │
     │ ○ Tag:                              │
     │ ○ Commit:                           │
     │                                     │
     │         [OK]    [Cancel]            │
     └─────────────────────────────────────┘

  5. 点击 OK

  验证分支创建成功：
  右键 → TortoiseGit → Browse References
  应该能看到新分支：test-version-control

  或者右下角任务栏的TortoiseGit图标上会显示当前分支名。

---
  步骤2：创建MD文件

  操作：
  1. 在项目目录下，右键 → 新建 → 文本文档
  2. 重命名为：Git版本管理指南.md
  3. 用VS Code或记事本打开
  4. 复制下面的内容粘贴进去

  文件内容（完整版）：

  # Git 版本管理完全指南

  ## 项目信息

  **文档创建日期**：2025-11-20
  **适用项目**：易播服务器 - Socket封装学习
  **项目路径**：`D:\VS\learning_linux_11_6\learning_linux_11_6`

---

  ## 一、版本回退方法

  ### 1.1 打开历史记录

  在 TortoiseGit 中查看提交历史：
  右键 → TortoiseGit → Show log

---

  ### 1.2 三种回退方式

  #### 方式1：Revert（撤销提交）⭐ 最安全

  **适用场景**：
  - ✅ 已经推送到远程的提交
  - ✅ 多人协作项目
  - ✅ 想保留完整历史记录

  **操作步骤**：
  1. Show log 中找到要撤销的提交
  2. 右键该提交 → "Revert change by this commit"
  3. 会创建一个新的提交，内容是撤销之前的更改
  4. Push到远程（如果需要）

  **效果**：
  原始历史：A → B → C (HEAD)
  执行 Revert C 后：A → B → C → C'(HEAD)
                          ↑   ↑
                       错误的 撤销提交

---

  #### 方式2：Reset（重置）⚠️ 危险但彻底

  **三种模式**：

  **① Reset Mixed（默认）- 保留工作区修改**

  适用场景：
  - ✅ 只在本地提交，还没Push
  - ✅ 想修改提交内容重新提交

  操作步骤：
  1. Show log 中右键目标提交
  2. Reset 'main' to this...
  3. 选择 "Mixed: Leave working tree untouched, reset index"
  4. 点击 OK

  效果：
  - 工作区文件：保留修改（未暂存）
  - 暂存区：清空
  - 提交历史：后续提交消失

---

  **② Reset Soft - 保留暂存区**

  适用场景：
  - ✅ 合并多个提交为一个

  操作步骤：
  1. Show log 中右键目标提交
  2. Reset 'main' to this...
  3. 选择 "Soft: Leave working tree and index untouched"
  4. 点击 OK

  效果：
  - 工作区文件：保留修改（已暂存）
  - 暂存区：保留修改
  - 提交历史：后续提交消失

---

  **③ Reset Hard - 完全丢弃修改 ⛔ 最危险**

  适用场景：
  - ⚠️ 确定要完全放弃某些提交

  操作步骤：
  1. Show log 中右键目标提交
  2. Reset 'main' to this...
  3. 选择 "Hard: Reset working tree and index"
  4. 确认警告
  5. 点击 OK

  效果：
  - ⚠️ 工作区文件会丢失
  - ⚠️ 所有修改永久丢失
  - ❌ 绝不能对已Push的提交使用

---

  #### 方式3：Checkout（检出）- 临时查看

  **适用场景**：
  - ✅ 只是想查看旧版本代码
  - ✅ 不想改变历史

  **操作步骤**：
  1. Show log 中找到想查看的提交
  2. 右键 → "Switch/Checkout to revision..."
  3. 确认切换

  返回当前版本：
  右键 → Switch/Checkout → 选择 main 分支

---

  ## 二、一天一个版本的管理策略

  ### 2.1 方案1：使用分支（Branch）⭐ 最推荐

  **每天创建一个新分支，保留完整的每日快照。**

  #### 操作流程

  **第1步：创建今天的分支**
  1. 右键 → TortoiseGit → Create Branch
  2. 分支名：day-2025-11-20（或 day1, day2, day3...）
  3. 勾选 "Switch to new branch"
  4. 点击 OK

  **第2步：在当天分支上工作**
  正常修改代码 → Commit → Push

  **第3步：第二天开始新的一天**
  1. 切换回主分支：
    右键 → Switch/Checkout → 选择 "main" → OK
  2. 创建新的每日分支：
    右键 → Create Branch → day-2025-11-21
    勾选 "Switch to new branch" → OK

  **查看某一天的代码**
  右键 → Switch/Checkout → 选择想查看的分支

  #### 分支命名建议

  ```bash
  # 方案A：按日期
  day-2025-11-20
  day-2025-11-21

  # 方案B：按天数
  day1-buffer实现
  day2-csockparam实现

  # 方案C：按功能模块
  socket-day1-基础类
  socket-day2-init实现

  ---
  2.2 方案2：使用标签（Tag）⭐ 最简单

  为每天的最后一次提交打上标签。

  操作流程

  每天结束时打标签
  1. 右键 → TortoiseGit → Create Tag
  2. 标签名：v2025-11-20 或 day1
  3. 填写标签信息：
     "完成Buffer、CSockParam、Init()函数实现"
  4. 点击 OK

  推送标签到远程
  1. 右键 → TortoiseGit → Push
  2. 勾选 "Include Tags"
  3. 点击 OK

  查看某个标签的代码
  1. 右键 → Show log
  2. 勾选 "All branches + tags"
  3. 找到标签，右键 → Switch/Checkout

  标签命名建议

  # 方案A：按日期版本
  v2025.11.20
  v2025.11.21

  # 方案B：语义化版本
  v0.1.0-buffer实现
  v0.2.0-csockparam实现

  # 方案C：里程碑
  milestone-基础类完成
  milestone-init完成

  ---
  2.3 方案3：规范提交信息 + 每日总结

  在main分支上工作，通过规范的提交信息标记。

  操作流程

  每天最后一次提交时
  提交信息格式：
  [Day 1] 完成Buffer、SockAttr、CSockParam类实现

  详细说明：
  - Buffer类：继承std::string，三个转换运算符
  - SockAttr：位标志枚举
  - CSockParam：双地址结构体
  - 学习时长：3小时

  ---
  2.4 方案4：混合策略（推荐给学习项目）

  结合分支和标签，既保留灵活性又有清晰的里程碑。

  结构设计

  main（主分支）
   │
   ├─ day1 分支
   │   └─ v0.1.0 tag（Buffer、SockAttr实现）
   │
   ├─ day2 分支
   │   └─ v0.2.0 tag（CSockParam、CSocketBase实现）
   │
   ├─ day3 分支
   │   └─ v0.3.0 tag（Init()函数实现）

  操作流程

  每天开始
  1. 从main创建新分支：day-X
  2. 在新分支上工作
  3. 随时提交

  每天结束
  1. 打标签：v0.X.0
  2. 推送分支和标签
  3. （可选）合并回main

  ---
  三、查看历史版本

  3.1 通过分支切换

  1. 右键 → TortoiseGit → Switch/Checkout
  2. 选择分支（如day1-基础类实现）
  3. 点击OK
  4. 代码立即切换到那个版本

  返回最新版本：
  Switch/Checkout → main

  ---
  3.2 通过标签切换

  1. 右键 → Show log
  2. 勾选"All branches + tags"
  3. 找到标签（如day1-基础类完成）
  4. 右键 → Switch/Checkout

  ---
  3.3 通过提交记录（不切换）

  1. 右键 → Show log
  2. 双击任意提交
  3. 查看文件列表
  4. 双击文件 → 查看那个版本的内容
  5. 不会改变当前工作区

  ---
  3.4 对比两个版本

  方法1：对比当前版本
  1. 右键 → Show log
  2. 选中历史版本
  3. 右键 → Compare with working tree

  方法2：对比两个历史版本
  1. 选中版本A
  2. 按住Ctrl，选中版本B
  3. 右键 → Compare revisions

  ---
  四、快速决策表

  | 场景      | 是否已Push | 推荐方式       | 操作                   |
  |---------|---------|------------|----------------------|
  | 撤销某个提交  | ✅ 是     | Revert     | 右键提交 → Revert        |
  | 修改最后提交  | ❌ 否     | Reset Soft | 右键前一个提交 → Reset Soft |
  | 合并多个提交  | ❌ 否     | Reset Soft | 右键目标提交 → Reset Soft  |
  | 完全放弃修改  | ❌ 否     | Reset Hard | 右键目标提交 → Reset Hard  |
  | 只是查看旧版本 | 无所谓     | Checkout   | 右键提交 → Checkout      |

  ---
  五、推荐方案

  给学习项目的推荐：标签方案

  理由：
  1. ✅ 简单易用，不需要频繁切换分支
  2. ✅ GitHub自动识别为Release
  3. ✅ 可以为每个标签写详细说明
  4. ✅ 不影响线性的学习进度

  具体操作

  从明天开始
  【每天晚上】
  1. 确保所有修改已提交
  2. 右键 → Create Tag
     格式：day2 或 v2025.11.21
     信息：今天完成的内容总结
  3. 右键 → Push（勾选Include Tags）
  4. 在学习总结.md中记录今天的进度

  查看某一天的版本
  1. 右键 → Show log
  2. 勾选"All branches + tags"
  3. 找到标签，右键 → Checkout
  4. 查看完毕后，切换回main

  ---
  六、最佳实践建议

  ✅ 推荐做法

  1. 本地提交未Push：
    - 用 Reset Mixed 或 Reset Soft 修改
    - 安全，灵活
  2. 已Push到远程：
    - 用 Revert 撤销
    - 永远不要用 Reset + Force Push
  3. 多人协作：
    - 只用 Revert
    - 保持历史清晰
  4. 实验性代码：
    - 创建新分支测试
    - 不满意直接删除分支

  ---
  ❌ 避免的操作

  1. 对已Push的提交用 Reset Hard ⛔
    - 会导致团队成员代码混乱
  2. 强制推送到主分支 ⛔
    - 除非你是唯一开发者
  3. 没有备份就用 Reset Hard ⛔
    - 至少先创建一个备份分支

  ---
  七、安全回退流程

  【第1步】创建备份分支（可选但推荐）
  右键 → Create Branch → backup-YYYY-MM-DD

  【第2步】查看历史
  右键 → Show log

  【第3步】选择回退方式
  - 已Push：用 Revert
  - 未Push：用 Reset Mixed/Soft

  【第4步】验证结果
  查看代码是否符合预期

  【第5步】推送（如果需要）
  - Revert：直接Push
  - Reset：只在确保未Push时才推送

  【第6步】删除备份分支（可选）
  如果确认无误，可以删除备份分支

  ---
  八、撤销回退（后悔药）

  使用 Reflog（引用日志）

  1. 右键 → TortoiseGit → Show Reflog
  2. 找到被删除的提交（记录所有HEAD的变动）
  3. 右键该提交 → "Reset 'main' to this..."
  4. 选择适当的模式
  5. OK

  注意：Reflog 只保留30天（默认），过期会被清理。

  ---
  九、本文档创建记录

  创建目的：测试分支管理功能
  分支名称：test-version-control
  创建日期：2025-11-20
  测试内容：
  - ✅ 创建新分支
  - ⏳ 在新分支上添加文件
  - ⏳ 提交修改
  - ⏳ 推送到远程
  - ⏳ 切换回主分支
  - ⏳ 查看分支差异

  ---
  更新日期：2025-11-20
  文档版本：v1.0
  适用项目：易播服务器 Socket封装学习

  ---

  ## 步骤3：添加文件到Git

  **操作**：
  1. 确认文件已保存：Git版本管理指南.md
  2. 在项目目录右键 → Git Commit -> "test-version-control"
  （注意：右上角会显示当前分支名）

  **在提交窗口中**：
  ┌─────────────────────────────────────────────┐
  │ Commit - test-version-control               │ ← 确认分支名
  ├─────────────────────────────────────────────┤
  │ Message:                                    │
  │ ┌─────────────────────────────────────────┐ │
  │ │ 【测试】添加Git版本管理指南文档          │ │
  │ │                                          │ │
  │ │ - 创建测试分支 test-version-control      │ │
  │ │ - 添加版本管理完整指南                    │ │
  │ │ - 包含回退、分支、标签等操作说明          │ │
  │ └─────────────────────────────────────────┘ │
  │                                             │
  │ Changes to be committed:                    │
  │ ☑ Git版本管理指南.md                        │ ← 确认勾选
  │                                             │
  │ ☐ Push                                      │ ← 先不勾选
  │                                             │
  │        [Commit]         [Cancel]            │
  └─────────────────────────────────────────────┘

  3. 点击 **Commit** 按钮

  ---

  ## 步骤4：推送到远程

  **操作**：
  1. 提交成功后，右键 → TortoiseGit → Push
  2. 在Push窗口中：
  ┌─────────────────────────────────────┐
  │ Push - TortoiseGit                  │
  ├─────────────────────────────────────┤
  │ Local:  [test-version-control]      │ ← 确认是新分支
  │ Remote: [origin]                    │
  │ Remote Branch: [test-version-control]│
  │                                     │
  │ ☐ Force (May discard changes)      │ ← 不要勾选
  │ ☐ Push all branches                │
  │ ☐ Include Tags                     │
  │                                     │
  │        [OK]    [Cancel]             │
  └─────────────────────────────────────┘
  3. 点击 OK
  4. 等待推送完成（会显示进度条）

  ---

  ## 步骤5：验证推送成功

  **方法1：查看本地日志**
  1. 右键 → Show log
  2. 确认最新提交有 "↑" 符号（表示已推送）
  3. 查看右上角分支名：test-version-control

  **方法2：在GitHub查看**
  1. 打开浏览器
  2. 访问：https://github.com/你的用户名/你的仓库名
  3. 点击分支选择器（默认显示 main）
  4. 应该能看到新分支：test-version-control
  5. 切换到该分支，确认有 Git版本管理指南.md 文件

  ---

  ## 步骤6：切换回主分支

  **操作**：
  1. 右键 → TortoiseGit → Switch/Checkout
  2. 选择分支：main
  3. 点击 OK
  4. 查看项目目录：Git版本管理指南.md 应该消失了
  （因为这个文件只在 test-version-control 分支存在）

  **验证**：
  右键 → Show log
  右上角应该显示：main

  ---

  ## 步骤7：对比两个分支的差异

  **操作**：
  1. 右键 → Show log
  2. 在上方输入框输入：test-version-control
  3. 回车，查看该分支的提交
  4. 找到刚才的提交，双击
  5. 可以看到添加的文件内容

  **或者图形化对比**：
  1. 右键 → TortoiseGit → Browse References
  2. 左侧选择 Branches → test-version-control
  3. 右键 → Compare with main
  4. 会显示两个分支的差异

  ---

  ## 步骤8：（可选）切换回测试分支继续修改

  **操作**：
  1. 右键 → Switch/Checkout → test-version-control
  2. 查看项目目录：Git版本管理指南.md 又出现了
  3. 可以继续修改、提交
  4. 完成后再切换回 main

  ---

  ## 完成测试后的选择

  ### 选项A：保留测试分支（推荐）

  保持现状，测试分支继续存在
  以后可以用来存放文档或测试代码

  ---

  ### 选项B：合并到主分支

  **如果觉得这个文档很有用，想加入主分支**：

  1. 切换到 main 分支：
  右键 → Switch/Checkout → main
  2. 合并测试分支：
  右键 → TortoiseGit → Merge
  选择 test-version-control
  点击 OK
  3. 如果没有冲突，合并成功
  4. Push到远程：
  右键 → Push

  结果：main分支也有 Git版本管理指南.md 了

  ---

  ### 选项C：删除测试分支

  **如果只是测试，不需要保留**：

  【本地删除】
  1. 确保已切换到 main 分支
  2. 右键 → TortoiseGit → Browse References
  3. 找到 Branches → test-version-control
  4. 右键 → Delete
  5. 确认删除

  【远程删除】
  1. 右键 → TortoiseGit → Browse References
  2. 展开 Remotes → origin → test-version-control
  3. 右键 → Delete remote branch
  4. 确认删除

  或者命令行：
  git push origin --delete test-version-control

  ---

  ## 现在开始操作吧！

  **从步骤1开始执行，每完成一步告诉我**，我会确认你的操作是否正确，然后指导下一步。

  **有任何疑问或遇到提示框和你看到的不一样，立即截图或描述给我！**

  准备好了吗？开始 **步骤1：创建测试分支** 吧！ 🚀