/*
 * Copyright (c) 2026 Huawei Technologies Co., Ltd. All Rights Reserved.
 *
 * KUPL is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *        http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */
#ifndef KUPL_GLIBC_VERSION_H
#define KUPL_GLIBC_VERSION_H

#ifdef __cplusplus
extern "C" {
#endif

__asm__(".symver dladdr, dladdr@");
__asm__(".symver dlclose, dlclose@");
__asm__(".symver dlerror, dlerror@");
__asm__(".symver dlopen, dlopen@");
__asm__(".symver dlsym, dlsym@");
__asm__(".symver glob, glob@");
__asm__(".symver pthread_setaffinity_np, pthread_setaffinity_np@");
__asm__(".symver pthread_getaffinity_np, pthread_getaffinity_np@");
__asm__(".symver pthread_attr_setstacksize, pthread_attr_setstacksize@");
__asm__(".symver pthread_create, pthread_create@");
__asm__(".symver pthread_join, pthread_join@");
__asm__(".symver pthread_mutex_trylock, pthread_mutex_trylock@");
__asm__(".symver __pthread_key_create, __pthread_key_create@");
__asm__(".symver pthread_spin_destroy, pthread_spin_destroy@");
__asm__(".symver pthread_spin_init, pthread_spin_init@");
__asm__(".symver pthread_spin_lock, pthread_spin_lock@");
__asm__(".symver pthread_spin_trylock, pthread_spin_trylock@");
__asm__(".symver pthread_spin_unlock, pthread_spin_unlock@");
__asm__(".symver shm_open, shm_open@");
__asm__(".symver shm_unlink, shm_unlink@");
__asm__(".symver __isoc23_strtol, strtol@");

#ifdef __cplusplus
}
#endif
#endif