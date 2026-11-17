#include <typedefs.h>
#include <linuxver.h>
#include <osl.h>

#include <proto/ethernet.h>
#include <dngl_stats.h>
#include <bcmutils.h>
#include <dhd.h>
#include <dhd_dbg.h>

#include <linux/fcntl.h>
#include <linux/fs.h>
#include <linux/etherdevice.h>
#include "dhd_custom_sec_mac.h"

struct dhd_info;
extern int _dhd_set_mac_address(struct dhd_info *dhd,
					int ifidx, struct ether_addr *addr);

#define MACINFO "/data/.mac.info"
#define PSMINFO "/data/.psm.info"

#ifdef READ_MACADDR
int dhd_read_macaddr(struct dhd_info *dhd, struct ether_addr *mac)
{
	struct file *fp;
	char text[19] = { 0 };
	mm_segment_t oldfs;
	int count, ret;

	fp = filp_open(MACINFO, O_RDONLY, 0);
	if (!IS_ERR(fp)) {
		count = kernel_read(fp, 0, text, sizeof(text) - 1);
		filp_close(fp, NULL);
		if (count > 0 && !dhd_parse_mac(text, count, mac->octet))
			goto apply_mac;
	}

	/* Preserve a valid saved address; create a local unicast address otherwise. */
	random_ether_addr(mac->octet);
	count = scnprintf(text, sizeof(text), "%pM\n", mac->octet);
	fp = filp_open(MACINFO, O_WRONLY | O_CREAT | O_TRUNC, 0600);
	if (!IS_ERR(fp)) {
		oldfs = get_fs();
		set_fs(get_ds());
		ret = fp->f_op->write ?
			fp->f_op->write(fp, text, count, &fp->f_pos) : -EINVAL;
		set_fs(oldfs);
		filp_close(fp, NULL);
		if (ret != count)
			DHD_ERROR(("[WIFI] Could not persist MAC address (%d)\n", ret));
	} else {
		DHD_ERROR(("[WIFI] Could not open MAC file for writing\n"));
	}

apply_mac:
	ret = _dhd_set_mac_address(dhd, 0, mac);
	if (ret)
		DHD_ERROR(("[WIFI] _dhd_set_mac_address failed (%d)\n", ret));
	return ret;
}
#endif /* READ_MACADDR */

#ifdef WRITE_MACADDR
int dhd_write_macaddr(struct ether_addr *mac)
{
    char *filepath_data      = MACINFO;

	struct file *fp_mac = NULL;
	char buf[19]      = {0};
	mm_segment_t oldfs    = {0};
	int ret = -1;
	int retry_count = 0;

startwrite:

	scnprintf(buf, sizeof(buf), "%pM\n", mac->octet);

	/* File will be created /data/.mac.info. */
	fp_mac = filp_open(filepath_data, O_RDWR | O_CREAT, 0666);

	if (IS_ERR(fp_mac)) {
		DHD_ERROR(("[WIFI] %s: File open error\n", filepath_data));
		return -1;
	} else {
		oldfs = get_fs();
		set_fs(get_ds());

		if (fp_mac->f_mode & FMODE_WRITE) {
			ret = fp_mac->f_op->write(fp_mac, (const char *)buf,
				strlen(buf), &fp_mac->f_pos);
			if (ret < 0)
				DHD_ERROR(("[WIFI] Mac address [%s] Failed to"
				" write into File: %s\n", buf, filepath_data));
			else
				DHD_INFO(("[WIFI] Mac address [%s] written"
				" into File: %s\n", buf, filepath_data));
		}
		set_fs(oldfs);
		filp_close(fp_mac, NULL);
	}
	/* check .mac.info file is 0 byte */
	fp_mac = filp_open(filepath_data, O_RDONLY, 0);
	if (IS_ERR(fp_mac))
		return PTR_ERR(fp_mac);
	ret = kernel_read(fp_mac, 0, buf, 18);

	if ((ret == 0) && (retry_count++ < 3)) {
		filp_close(fp_mac, NULL);
		goto startwrite;
	}

	filp_close(fp_mac, NULL);
	/* end of /data/.mac.info */

	return 0;
}
#endif /* WRITE_MACADDR */

#ifdef CONFIG_CONTROL_PM
extern bool g_pm_control;
void sec_control_pm(dhd_pub_t *dhd, uint *power_mode)
{
	struct file *fp = NULL;
	char *filepath = PSMINFO;
	mm_segment_t oldfs = {0};
	char power_val = 0;
	char iovbuf[WL_EVENTING_MASK_LEN + 12];

	g_pm_control = FALSE;

	fp = filp_open(filepath, O_RDONLY, 0);
	if (IS_ERR(fp)) {
		/* Enable PowerSave Mode */
		dhd_wl_ioctl_cmd(dhd, WLC_SET_PM, (char *)power_mode,
			sizeof(uint), TRUE, 0);

		fp = filp_open(filepath, O_RDWR | O_CREAT, 0666);
		if (IS_ERR(fp) || (fp == NULL)) {
			DHD_ERROR(("[%s, %d] /data/.psm.info open failed\n",
				__FUNCTION__, __LINE__));
			return;
		} else {
			oldfs = get_fs();
			set_fs(get_ds());

			if (fp->f_mode & FMODE_WRITE) {
				power_val = '1';
				fp->f_op->write(fp, (const char *)&power_val,
					sizeof(char), &fp->f_pos);
			}
			set_fs(oldfs);
		}
	} else {
		kernel_read(fp, fp->f_pos, &power_val, 1);
		DHD_ERROR(("POWER_VAL = %c \r\n" , power_val));

		if (power_val == '0') {
#ifdef ROAM_ENABLE
			uint roamvar = 1;
#endif
			*power_mode = PM_OFF;
			/* Disable PowerSave Mode */
			dhd_wl_ioctl_cmd(dhd, WLC_SET_PM, (char *)power_mode,
				sizeof(uint), TRUE, 0);
			/* Turn off MPC in AP mode */
			bcm_mkiovar("mpc", (char *)power_mode, 4,
					iovbuf, sizeof(iovbuf));
			dhd_wl_ioctl_cmd(dhd, WLC_SET_VAR, iovbuf,
				sizeof(iovbuf), TRUE, 0);
			g_pm_control = TRUE;
#ifdef ROAM_ENABLE
			/* Roaming off of dongle */
			bcm_mkiovar("roam_off", (char *)&roamvar, 4,
					iovbuf, sizeof(iovbuf));
			dhd_wl_ioctl_cmd(dhd, WLC_SET_VAR, iovbuf,
				sizeof(iovbuf), TRUE, 0);
#endif
		} else {
			dhd_wl_ioctl_cmd(dhd, WLC_SET_PM, (char *)power_mode,
				sizeof(uint), TRUE, 0);
		}
	}

	if (fp)
		filp_close(fp, NULL);
}
#endif
