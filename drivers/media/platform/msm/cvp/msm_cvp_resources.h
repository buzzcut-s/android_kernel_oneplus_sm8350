/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2018-2020, The Linux Foundation. All rights reserved.
 */

#ifndef __MSM_CVP_RESOURCES_H__
#define __MSM_CVP_RESOURCES_H__

#include <linux/devfreq.h>
#include <linux/platform_device.h>
#include "msm_cvp_core.h"
#include <linux/soc/qcom/llcc-qcom.h>

struct reg_value_pair {
	u32 reg;
	u32 value;
};

struct reg_set {
	struct reg_value_pair *reg_tbl;
	int count;
};

struct addr_range {
	u32 start;
	u32 size;
};

struct addr_set {
	struct addr_range *addr_tbl;
	int count;
};

struct context_bank_info {
	struct list_head list;
	const char *name;
	u32 buffer_type;
	bool is_secure;
	struct addr_range addr_range;
	struct device *dev;
	struct iommu_domain *domain;
};

struct regulator_info {
	struct regulator *regulator;
	bool has_hw_power_collapse;
	char *name;
};

struct regulator_set {
	struct regulator_info *regulator_tbl;
	u32 count;
};

struct clock_info {
	const char *name;
	struct clk *clk;
	u32 count;
	bool has_scaling;
	bool has_mem_retention;
};

struct clock_set {
	struct clock_info *clock_tbl;
	u32 count;
};

struct bus_info {
	char *name;
	int master;
	int slave;
	unsigned int range[2];
	const char *governor;
	struct device *dev;
	struct devfreq_dev_profile devfreq_prof;
	struct devfreq *devfreq;
	struct icc_path *client;
	bool is_prfm_gov_used;
};

struct bus_set {
	struct bus_info *bus_tbl;
	u32 count;
};

enum power_state {
	CVP_POWER_INIT,
	CVP_POWER_ON,
	CVP_POWER_OFF,
	CVP_POWER_INVALID,
};

struct reset_info {
	struct reset_control *rst;
	enum power_state required_state;
	const char *name;
};

struct reset_set {
	struct reset_info *reset_tbl;
	u32 count;
};

struct allowed_clock_rates_table {
	u32 clock_rate;
};

struct clock_profile_entry {
	u32 codec_mask;
	u32 vpp_cycles;
	u32 vsp_cycles;
	u32 low_power_cycles;
};

struct clock_freq_table {
	struct clock_profile_entry *clk_prof_entries;
	u32 count;
};

struct subcache_info {
	const char *name;
	bool isactive;
	bool isset;
	struct llcc_slice_desc *subcache;
};

struct subcache_set {
	struct subcache_info *subcache_tbl;
	u32 count;
};

struct msm_cvp_mem_cdsp {
	struct device *dev;
};

struct msm_cvp_platform_resources {
	phys_addr_t firmware_base;
	phys_addr_t register_base;
	phys_addr_t ipcc_reg_base;
	uint32_t register_size;
	uint32_t ipcc_reg_size;
	uint32_t irq;
	uint32_t sku_version;
	struct allowed_clock_rates_table *allowed_clks_tbl;
	u32 allowed_clks_tbl_size;
	struct clock_freq_table clock_freq_tbl;
	bool sys_cache_present;
	bool sys_cache_res_set;
	struct subcache_set subcache_set;
	struct reg_set reg_set;
	struct addr_set qdss_addr_set;
	uint32_t max_load;
	struct platform_device *pdev;
	struct regulator_set regulator_set;
	struct clock_set clock_set;
	struct bus_set bus_set;
	struct reset_set reset_set;
	bool use_non_secure_pil;
	bool sw_power_collapsible;
	bool auto_pil;
	struct list_head context_banks;
	bool thermal_mitigable;
	const char *fw_name;
	const char *hfi_version;
	bool never_unload_fw;
	bool debug_timeout;
	uint32_t max_inst_count;
	uint32_t max_secure_inst_count;
	int msm_cvp_hw_rsp_timeout;
	int msm_cvp_dsp_rsp_timeout;
	int msm_cvp_firmware_unload_delay;
	uint32_t msm_cvp_pwr_collapse_delay;
	bool non_fatal_pagefaults;
	struct msm_cvp_mem_cdsp mem_cdsp;
	uint32_t vpu_ver;
	uint32_t fw_cycles;
	struct msm_cvp_ubwc_config_data *ubwc_config;
};

static inline bool is_iommu_present(struct msm_cvp_platform_resources *res)
{
	return !list_empty(&res->context_banks);
}

#endif

