# 4-Xiyou-LUG

#### 介绍
TOPIC_ID:4, TEAM_ID:1382578369, TEAM_NAME:Xiyou-LUG.

#### 软件架构
借助信号实现上下文切换。始终信号作为抢占点，时钟信号处理函数中判断当前任务的时间片是否到期，若未到期则返回，若到期进行任务切换。 
用户进程中创建任务。


#### 安装教程

1.  git clone https://github.com/Xiyou-LUG/src.git
2.  cd src/
3.  mkdir build

#### 使用说明

1.  初始化任务框架

init();

2.  创建任务

/**
 * task_start - 创建一个优先级为prio，名字为name的任务
 * @name: 任务名
 * @prio: 任务优先级
 * @func: 任务处理函数
 * @func_arg: 任务参数
 * **/
struct task_struct* task_start(char* name, int prio, task_func function, void* func_arg);

#### 参与贡献

1.  Fork 本仓库
2.  新建 Feat_xxx 分支
3.  提交代码
4.  新建 Pull Request
