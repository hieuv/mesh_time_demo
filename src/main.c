/*
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/sys/printk.h>
#include <stdlib.h>
#include <stdint.h>
#include <zephyr/kernel.h>

#include <zephyr/shell/shell.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/mesh.h>
#include <zephyr/bluetooth/mesh/shell.h>

#include <bluetooth/mesh/models.h>

static struct bt_mesh_cfg_cli cfg_cli;

void dtt_update_handler(struct bt_mesh_dtt_srv *srv,
			     struct bt_mesh_msg_ctx *ctx,
			     uint32_t old_transition_time,
			     uint32_t new_transition_time) {}

static struct bt_mesh_time_srv time_srv = BT_MESH_TIME_SRV_INIT(NULL);
static struct bt_mesh_time_cli time_cli = BT_MESH_TIME_CLI_INIT(NULL);

BT_MESH_SHELL_HEALTH_PUB_DEFINE(health_pub);

static const struct bt_mesh_model root_models[] = {
	BT_MESH_MODEL_CFG_SRV,
	BT_MESH_MODEL_CFG_CLI(&cfg_cli),
	BT_MESH_MODEL_HEALTH_SRV(&bt_mesh_shell_health_srv, &health_pub, health_srv_meta),
	BT_MESH_MODEL_HEALTH_CLI(&bt_mesh_shell_health_cli),
};

static const struct bt_mesh_model client_models[] = {
	BT_MESH_MODEL_TIME_CLI(&time_cli),
};

static const struct bt_mesh_model server_models[] = {
	BT_MESH_MODEL_TIME_SRV(&time_srv),
};

static const struct bt_mesh_elem elements[] = {
	BT_MESH_ELEM(0, root_models, BT_MESH_MODEL_NONE),
	BT_MESH_ELEM(1, client_models, BT_MESH_MODEL_NONE),
	BT_MESH_ELEM(2, server_models, BT_MESH_MODEL_NONE),
};

static const struct bt_mesh_comp comp = {
	.cid = CONFIG_BT_COMPANY_ID,
	.elem = elements,
	.elem_count = ARRAY_SIZE(elements),
};

static void bt_ready(int err)
{
	if (err && err != -EALREADY) {
		printk("Bluetooth init failed (err %d)\n", err);
		return;
	}

	printk("Bluetooth initialized\n");

	err = bt_mesh_init(&bt_mesh_shell_prov, &comp);
	if (err) {
		printk("Initializing mesh failed (err %d)\n", err);
		return;
	}

	if (IS_ENABLED(CONFIG_SETTINGS)) {
		settings_load();
	}

	printk("Mesh initialized\n");

	if (bt_mesh_is_provisioned()) {
		printk("Mesh network restored from flash\n");
	} else {
		printk("Use \"prov pb-adv on\" or \"prov pb-gatt on\" to "
			    "enable advertising\n");
	}
}

int main(void)
{
	int err;

	printk("Initializing...\n");

	/* Initialize the Bluetooth Subsystem */
	err = bt_enable(bt_ready);
	if (err && err != -EALREADY) {
		printk("Bluetooth init failed (err %d)\n", err);
	}

	printk("Press the <Tab> button for supported commands.\n");
	printk("Before any Mesh commands you must run \"mesh init\"\n");
	return 0;
}

int demo_time_authority_time_set_cmd(const struct shell *sh, size_t argc, char **argv)
{
	int err;

	struct bt_mesh_time_status time_status = {
		.tai = {                    // For demo purpose, set TAI time to 100000:0
			.sec = 100000,
			.subsec = 0,
		},
		.uncertainty = 0,
		.tai_utc_delta = 292,       // Current TAI-UTC Delta (leap seconds) in spec representation
		.time_zone_offset = 0x44,   // +1:00 for Norway (with Daylight Saving Time)
		.is_authority = true        // Reliable TAI source flag
	};

	int64_t uptime = k_uptime_get();

	bt_mesh_time_srv_time_set(&time_srv, uptime, &time_status);

	printk("TAI time set to 100000:0\n");
	printk("Uptime: %lldms\n", uptime);

	return 0;
}

int demo_time_get_cmd(const struct shell *sh, size_t argc, char **argv)
{
	int err;
	int64_t uptime;
	uint64_t tai_sec;
	uint8_t tai_subsec;
	struct bt_mesh_time_status time_status;

	uptime = k_uptime_get();

	err = bt_mesh_time_srv_status(&time_srv, uptime, &time_status);

	if (err) {
		if (err == -EAGAIN) {
			printk("bt_mesh_time_srv_status() error -EAGAIN. Has time been set yet?\n");
		}
		else {
			printk("bt_mesh_time_srv_status() error %d\n", err);
		}
	}
	else {
		printk("TAI time extracted directly from Time Server: ");
		tai_sec = time_status.tai.sec;
		tai_subsec = time_status.tai.subsec;
		printk("%lld:%d\n", tai_sec, tai_subsec);
		printk("Uptime: %lldms\n", uptime);
	}
	return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(
		demo_subcmds,
		SHELL_CMD_ARG(time_authority_set_time, 
			NULL,
			"Set time on the Time Server, \
			meant to be used on node with Time Authority Role only\n",
			demo_time_authority_time_set_cmd, 1, 0),

		SHELL_CMD_ARG(local_time_server_get_time, 
			NULL,
			"Get time from the local Time Server\n",
			demo_time_get_cmd, 1, 0),
		SHELL_SUBCMD_SET_END
		);
SHELL_CMD_REGISTER(demo, &demo_subcmds, "Demo commands\n", NULL);
