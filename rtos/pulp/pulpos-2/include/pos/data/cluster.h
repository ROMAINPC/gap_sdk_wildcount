/*
 * Copyright (C) 2019 GreenWaves Technologies
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/* 
 * Authors: Germain Haugou, GreenWaves Technologies (germain.haugou@greenwaves-technologies.com)
 */

#ifndef __POS_DATA_CLUSTER_H__
#define __POS_DATA_CLUSTER_H__

#ifndef LANGUAGE_ASSEMBLY

typedef struct
{
    struct pi_cluster_task *first_call_fc_for_cl;
    struct pi_cluster_task *first_tasklet_fc_for_cl;
    pi_cl_workitem_t *first_call_from_pe;
    pi_cl_workitem_t *last_call_from_pe;
    pi_cl_workitem_t *first_pe_task;
    pi_cl_workitem_t *last_pe_task;
    uint32_t trig_addr;
} pos_cluster_call_pool_t;


typedef struct pos_cluster_t {
    struct pi_cluster_task *last_call_fc;
    struct pi_cluster_task *last_tasklet_fc;
    pos_cluster_call_pool_t *pool;
    void *cc_stack;
    void *stacks;
    int stacks_size;
    unsigned int trig_addr;
    pi_task_t *cl_tasks;
    uint32_t task_trig_addr;
    uint8_t cid;
    uint8_t cluster_exec_mode;
    uint8_t stack_set;
} pos_cluster_t;


extern PI_CL_L1_TINY int pos_pending_task;
extern PI_CL_L1_TINY pos_cluster_call_pool_t pos_cluster_pool;


#endif

#define POS_CLUSTER_CALL_POOL_T_FIRST_CALL_FC_FOR_CL    (0*4)
#define POS_CLUSTER_CALL_POOL_T_FIRST_TASKLET_FC_FOR_CL (1*4)
#define POS_CLUSTER_CALL_POOL_T_FIRST_CALL_FROM_PE      (2*4)
#define POS_CLUSTER_CALL_POOL_T_LAST_CALL_FROM_PE       (3*4)
#define POS_CLUSTER_CALL_POOL_T_FIRST_PE_TASK           (4*4)
#define POS_CLUSTER_CALL_POOL_T_LAST_PE_TASK            (5*4)
#define POS_CLUSTER_CALL_POOL_T_TRIG_ADDR               (6*4)

#define POS_CLUSTER_T_SIZEOF           (10*4)
#define POS_CLUSTER_T_LAST_CALL_FC      0
#define POS_CLUSTER_T_LAST_TASKLET_FC   4
#define POS_CLUSTER_T_POOL              8
#define POS_CLUSTER_T_CC_STACK          12
#define POS_CLUSTER_T_STACKS            16
#define POS_CLUSTER_T_STACKS_SIZE       20
#define POS_CLUSTER_T_TRIG_ADDR         24
#define POS_CLUSTER_T_CL_TASKS          28
#define POS_CLUSTER_T_TASK_TRIG_ADDR    32
#define POS_CLUSTER_T_CID               36
#define POS_CLUSTER_T_CLUSTER_EXEC_MODE 37
#define POS_CLUSTER_T_CLUSTER_STACK_SET 38

#endif