/*
 * Copyright 2014-2015 Freescale Semiconductor
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <fsl_ifc.h>
#include <ahci.h>
#include <scsi.h>
#include <asm/arch/fsl_serdes.h>
#include <asm/arch/soc.h>
#include <asm/io.h>
#include <asm/global_data.h>
#include <asm/arch-fsl-layerscape/config.h>
#ifdef CONFIG_LAYERSCAPE_NS_ACCESS
#include <fsl_csu.h>
#endif
#ifdef CONFIG_SYS_FSL_DDR
#include <fsl_ddr_sdram.h>
#include <fsl_ddr.h>
#endif
#ifdef CONFIG_CHAIN_OF_TRUST
#include <fsl_validate.h>
#endif

DECLARE_GLOBAL_DATA_PTR;

bool soc_has_dp_ddr(void)
{
	struct ccsr_gur __iomem *gur = (void *)(CONFIG_SYS_FSL_GUTS_ADDR);
	u32 svr = gur_in32(&gur->svr);

	/* LS2085A, LS2088A, LS2048A has DP_DDR */
	if ((SVR_SOC_VER(svr) == SVR_LS2085A) ||
	    (SVR_SOC_VER(svr) == SVR_LS2088A) ||
	    (SVR_SOC_VER(svr) == SVR_LS2048A))
		return true;

	return false;
}

bool soc_has_aiop(void)
{
	struct ccsr_gur __iomem *gur = (void *)(CONFIG_SYS_FSL_GUTS_ADDR);
	u32 svr = gur_in32(&gur->svr);

	/* LS2085A has AIOP */
	if (SVR_SOC_VER(svr) == SVR_LS2085A)
		return true;

	return false;
}

#if defined(CONFIG_FSL_LSCH3)
/*
 * This erratum requires setting a value to eddrtqcr1 to
 * optimal the DDR performance.
 */
static void erratum_a008336(void)
{
#ifdef CONFIG_SYS_FSL_ERRATUM_A008336
	u32 *eddrtqcr1;

#ifdef CONFIG_SYS_FSL_DCSR_DDR_ADDR
	eddrtqcr1 = (void *)CONFIG_SYS_FSL_DCSR_DDR_ADDR + 0x800;
	if (fsl_ddr_get_version(0) == 0x50200)
		out_le32(eddrtqcr1, 0x63b30002);
#endif
#ifdef CONFIG_SYS_FSL_DCSR_DDR2_ADDR
	eddrtqcr1 = (void *)CONFIG_SYS_FSL_DCSR_DDR2_ADDR + 0x800;
	if (fsl_ddr_get_version(0) == 0x50200)
		out_le32(eddrtqcr1, 0x63b30002);
#endif
#endif
}

/*
 * This erratum requires a register write before being Memory
 * controller 3 being enabled.
 */
static void erratum_a008514(void)
{
#ifdef CONFIG_SYS_FSL_ERRATUM_A008514
	u32 *eddrtqcr1;

#ifdef CONFIG_SYS_FSL_DCSR_DDR3_ADDR
	eddrtqcr1 = (void *)CONFIG_SYS_FSL_DCSR_DDR3_ADDR + 0x800;
	out_le32(eddrtqcr1, 0x63b20002);
#endif
#endif
}
#ifdef CONFIG_SYS_FSL_ERRATUM_A009635
#define PLATFORM_CYCLE_ENV_VAR	"a009635_interval_val"

static unsigned long get_internval_val_mhz(void)
{
	char *interval = getenv(PLATFORM_CYCLE_ENV_VAR);
	/*
	 *  interval is the number of platform cycles(MHz) between
	 *  wake up events generated by EPU.
	 */
	ulong interval_mhz = get_bus_freq(0) / (1000 * 1000);

	if (interval)
		interval_mhz = simple_strtoul(interval, NULL, 10);

	return interval_mhz;
}

void erratum_a009635(void)
{
	u32 val;
	unsigned long interval_mhz = get_internval_val_mhz();

	if (!interval_mhz)
		return;

	val = in_le32(DCSR_CGACRE5);
	writel(val | 0x00000200, DCSR_CGACRE5);

	val = in_le32(EPU_EPCMPR5);
	writel(interval_mhz, EPU_EPCMPR5);
	val = in_le32(EPU_EPCCR5);
	writel(val | 0x82820000, EPU_EPCCR5);
	val = in_le32(EPU_EPSMCR5);
	writel(val | 0x002f0000, EPU_EPSMCR5);
	val = in_le32(EPU_EPECR5);
	writel(val | 0x20000000, EPU_EPECR5);
	val = in_le32(EPU_EPGCR);
	writel(val | 0x80000000, EPU_EPGCR);
}
#endif	/* CONFIG_SYS_FSL_ERRATUM_A009635 */

static void erratum_rcw_src(void)
{
#if defined(CONFIG_SPL)
	u32 __iomem *dcfg_ccsr = (u32 __iomem *)DCFG_BASE;
	u32 __iomem *dcfg_dcsr = (u32 __iomem *)DCFG_DCSR_BASE;
	u32 val;

	val = in_le32(dcfg_ccsr + DCFG_PORSR1 / 4);
	val &= ~DCFG_PORSR1_RCW_SRC;
	val |= DCFG_PORSR1_RCW_SRC_NOR;
	out_le32(dcfg_dcsr + DCFG_DCSR_PORCR1 / 4, val);
#endif
}

#define I2C_DEBUG_REG 0x6
#define I2C_GLITCH_EN 0x8
/*
 * This erratum requires setting glitch_en bit to enable
 * digital glitch filter to improve clock stability.
 */
static void erratum_a009203(void)
{
	u8 __iomem *ptr;
#ifdef CONFIG_SYS_I2C
#ifdef I2C1_BASE_ADDR
	ptr = (u8 __iomem *)(I2C1_BASE_ADDR + I2C_DEBUG_REG);

	writeb(I2C_GLITCH_EN, ptr);
#endif
#ifdef I2C2_BASE_ADDR
	ptr = (u8 __iomem *)(I2C2_BASE_ADDR + I2C_DEBUG_REG);

	writeb(I2C_GLITCH_EN, ptr);
#endif
#ifdef I2C3_BASE_ADDR
	ptr = (u8 __iomem *)(I2C3_BASE_ADDR + I2C_DEBUG_REG);

	writeb(I2C_GLITCH_EN, ptr);
#endif
#ifdef I2C4_BASE_ADDR
	ptr = (u8 __iomem *)(I2C4_BASE_ADDR + I2C_DEBUG_REG);

	writeb(I2C_GLITCH_EN, ptr);
#endif
#endif
}

void bypass_smmu(void)
{
	u32 val;
	val = (in_le32(SMMU_SCR0) | SCR0_CLIENTPD_MASK) & ~(SCR0_USFCFG_MASK);
	out_le32(SMMU_SCR0, val);
	val = (in_le32(SMMU_NSCR0) | SCR0_CLIENTPD_MASK) & ~(SCR0_USFCFG_MASK);
	out_le32(SMMU_NSCR0, val);
}
void fsl_lsch3_early_init_f(void)
{
	erratum_rcw_src();
	init_early_memctl_regs();	/* tighten IFC timing */
	erratum_a009203();
	erratum_a008514();
	erratum_a008336();
#ifdef CONFIG_CHAIN_OF_TRUST
	/* In case of Secure Boot, the IBR configures the SMMU
	* to allow only Secure transactions.
	* SMMU must be reset in bypass mode.
	* Set the ClientPD bit and Clear the USFCFG Bit
	*/
	if (fsl_check_boot_mode_secure() == 1)
		bypass_smmu();
#endif
}

#ifdef CONFIG_SCSI_AHCI_PLAT
int sata_init(void)
{
	struct ccsr_ahci __iomem *ccsr_ahci;

	ccsr_ahci  = (void *)CONFIG_SYS_SATA2;
	out_le32(&ccsr_ahci->ppcfg, AHCI_PORT_PHY_1_CFG);
	out_le32(&ccsr_ahci->ptc, AHCI_PORT_TRANS_CFG);
	out_le32(&ccsr_ahci->axicc, AHCI_PORT_AXICC_CFG);

	ccsr_ahci  = (void *)CONFIG_SYS_SATA1;
	out_le32(&ccsr_ahci->ppcfg, AHCI_PORT_PHY_1_CFG);
	out_le32(&ccsr_ahci->ptc, AHCI_PORT_TRANS_CFG);
	out_le32(&ccsr_ahci->axicc, AHCI_PORT_AXICC_CFG);

	ahci_init((void __iomem *)CONFIG_SYS_SATA1);
	scsi_scan(0);

	return 0;
}
#endif

#elif defined(CONFIG_FSL_LSCH2)
#ifdef CONFIG_SCSI_AHCI_PLAT
int sata_init(void)
{
	struct ccsr_ahci __iomem *ccsr_ahci = (void *)CONFIG_SYS_SATA;

#ifdef CONFIG_ARCH_LS1046A
	/* Disable SATA ECC */
	out_le32((void *)CONFIG_SYS_DCSR_DCFG_ADDR + 0x520, 0x80000000);
#endif
	out_le32(&ccsr_ahci->ppcfg, AHCI_PORT_PHY_1_CFG);
	out_le32(&ccsr_ahci->ptc, AHCI_PORT_TRANS_CFG);
	out_le32(&ccsr_ahci->axicc, AHCI_PORT_AXICC_CFG);

	ahci_init((void __iomem *)CONFIG_SYS_SATA);
	scsi_scan(0);

	return 0;
}
#endif

static void erratum_a009929(void)
{
#ifdef CONFIG_SYS_FSL_ERRATUM_A009929
	struct ccsr_gur *gur = (void *)CONFIG_SYS_FSL_GUTS_ADDR;
	u32 __iomem *dcsr_cop_ccp = (void *)CONFIG_SYS_DCSR_COP_CCP_ADDR;
	u32 rstrqmr1 = gur_in32(&gur->rstrqmr1);

	rstrqmr1 |= 0x00000400;
	gur_out32(&gur->rstrqmr1, rstrqmr1);
	writel(0x01000000, dcsr_cop_ccp);
#endif
}

/*
 * This erratum requires setting a value to eddrtqcr1 to optimal
 * the DDR performance. The eddrtqcr1 register is in SCFG space
 * of LS1043A and the offset is 0x157_020c.
 */
#if defined(CONFIG_SYS_FSL_ERRATUM_A009660) \
	&& defined(CONFIG_SYS_FSL_ERRATUM_A008514)
#error A009660 and A008514 can not be both enabled.
#endif

static void erratum_a009660(void)
{
#ifdef CONFIG_SYS_FSL_ERRATUM_A009660
	u32 *eddrtqcr1 = (void *)CONFIG_SYS_FSL_SCFG_ADDR + 0x20c;
	out_be32(eddrtqcr1, 0x63b20042);
#endif
}

static void erratum_a008850_early(void)
{
#ifdef CONFIG_SYS_FSL_ERRATUM_A008850
	/* part 1 of 2 */
	struct ccsr_cci400 __iomem *cci = (void *)CONFIG_SYS_CCI400_ADDR;
	struct ccsr_ddr __iomem *ddr = (void *)CONFIG_SYS_FSL_DDR_ADDR;

	/* disables propagation of barrier transactions to DDRC from CCI400 */
	out_le32(&cci->ctrl_ord, CCI400_CTRLORD_TERM_BARRIER);

	/* disable the re-ordering in DDRC */
	ddr_out32(&ddr->eor, DDR_EOR_RD_REOD_DIS | DDR_EOR_WD_REOD_DIS);
#endif
}

void erratum_a008850_post(void)
{
#ifdef CONFIG_SYS_FSL_ERRATUM_A008850
	/* part 2 of 2 */
	struct ccsr_cci400 __iomem *cci = (void *)CONFIG_SYS_CCI400_ADDR;
	struct ccsr_ddr __iomem *ddr = (void *)CONFIG_SYS_FSL_DDR_ADDR;
	u32 tmp;

	/* enable propagation of barrier transactions to DDRC from CCI400 */
	out_le32(&cci->ctrl_ord, CCI400_CTRLORD_EN_BARRIER);

	/* enable the re-ordering in DDRC */
	tmp = ddr_in32(&ddr->eor);
	tmp &= ~(DDR_EOR_RD_REOD_DIS | DDR_EOR_WD_REOD_DIS);
	ddr_out32(&ddr->eor, tmp);
#endif
}

#ifdef CONFIG_SYS_FSL_ERRATUM_A010315
void erratum_a010315(void)
{
	int i;

	for (i = PCIE1; i <= PCIE4; i++)
		if (!is_serdes_configured(i)) {
			debug("PCIe%d: disabled all R/W permission!\n", i);
			set_pcie_ns_access(i, 0);
		}
}
#endif

static void erratum_a010539(void)
{
#if defined(CONFIG_SYS_FSL_ERRATUM_A010539) && defined(CONFIG_QSPI_BOOT)
	struct ccsr_gur __iomem *gur = (void *)(CONFIG_SYS_FSL_GUTS_ADDR);
	u32 porsr1;

	porsr1 = in_be32(&gur->porsr1);
	porsr1 &= ~FSL_CHASSIS2_CCSR_PORSR1_RCW_MASK;
	out_be32((void *)(CONFIG_SYS_DCSR_DCFG_ADDR + DCFG_DCSR_PORCR1),
		 porsr1);
#endif
}

/* Get VDD in the unit mV from voltage ID */
int get_core_volt_from_fuse(void)
{
	struct ccsr_gur *gur = (void *)(CONFIG_SYS_FSL_GUTS_ADDR);
	int vdd;
	u32 fusesr;
	u8 vid;

	fusesr = in_be32(&gur->dcfg_fusesr);
	debug("%s: fusesr = 0x%x\n", __func__, fusesr);
	vid = (fusesr >> FSL_CHASSIS2_DCFG_FUSESR_ALTVID_SHIFT) &
		FSL_CHASSIS2_DCFG_FUSESR_ALTVID_MASK;
	if ((vid == 0) || (vid == FSL_CHASSIS2_DCFG_FUSESR_ALTVID_MASK)) {
		vid = (fusesr >> FSL_CHASSIS2_DCFG_FUSESR_VID_SHIFT) &
			FSL_CHASSIS2_DCFG_FUSESR_VID_MASK;
	}
	debug("%s: VID = 0x%x\n", __func__, vid);
	switch (vid) {
	case 0x00: /* VID isn't supported */
		vdd = -EINVAL;
		debug("%s: The VID feature is not supported\n", __func__);
		break;
	case 0x08: /* 0.9V silicon */
		vdd = 900;
		break;
	case 0x10: /* 1.0V silicon */
		vdd = 1000;
		break;
	default:  /* Other core voltage */
		vdd = -EINVAL;
		printf("%s: The VID(%x) isn't supported\n", __func__, vid);
		break;
	}
	debug("%s: The required minimum volt of CORE is %dmV\n", __func__, vdd);

	return vdd;
}

__weak int board_switch_core_volt(u32 vdd)
{
	return 0;
}

static int setup_core_volt(u32 vdd)
{
	return board_setup_core_volt(vdd);
}

#ifdef CONFIG_SYS_FSL_DDR
static void ddr_enable_0v9_volt(bool en)
{
	struct ccsr_ddr __iomem *ddr = (void *)CONFIG_SYS_FSL_DDR_ADDR;
	u32 tmp;

	tmp = ddr_in32(&ddr->ddr_cdr1);

	if (en)
		tmp |= DDR_CDR1_V0PT9_EN;
	else
		tmp &= ~DDR_CDR1_V0PT9_EN;

	ddr_out32(&ddr->ddr_cdr1, tmp);
}
#endif

int setup_chip_volt(void)
{
	int vdd;

	vdd = get_core_volt_from_fuse();
	/* Nothing to do for silicons doesn't support VID */
	if (vdd < 0)
		return vdd;

	if (setup_core_volt(vdd))
		printf("%s: Switch core VDD to %dmV failed\n", __func__, vdd);
#ifdef CONFIG_SYS_HAS_SERDES
	if (setup_serdes_volt(vdd))
		printf("%s: Switch SVDD to %dmV failed\n", __func__, vdd);
#endif

#ifdef CONFIG_SYS_FSL_DDR
	if (vdd == 900)
		ddr_enable_0v9_volt(true);
#endif

	return 0;
}

void fsl_lsch2_early_init_f(void)
{
	struct ccsr_cci400 *cci = (struct ccsr_cci400 *)CONFIG_SYS_CCI400_ADDR;
	struct ccsr_scfg *scfg = (struct ccsr_scfg *)CONFIG_SYS_FSL_SCFG_ADDR;

#ifdef CONFIG_LAYERSCAPE_NS_ACCESS
	enable_layerscape_ns_access();
#endif

#ifdef CONFIG_FSL_IFC
	init_early_memctl_regs();	/* tighten IFC timing */
#endif

#if defined(CONFIG_FSL_QSPI) && !defined(CONFIG_QSPI_BOOT)
	out_be32(&scfg->qspi_cfg, SCFG_QSPI_CLKSEL);
#endif
	/* Make SEC reads and writes snoopable */
	setbits_be32(&scfg->snpcnfgcr, SCFG_SNPCNFGCR_SECRDSNP |
		     SCFG_SNPCNFGCR_SECWRSNP |
		     SCFG_SNPCNFGCR_SATARDSNP |
		     SCFG_SNPCNFGCR_SATAWRSNP);

	/*
	 * Enable snoop requests and DVM message requests for
	 * Slave insterface S4 (A53 core cluster)
	 */
	out_le32(&cci->slave[4].snoop_ctrl,
		 CCI400_DVM_MESSAGE_REQ_EN | CCI400_SNOOP_REQ_EN);

	/* Erratum */
	erratum_a008850_early(); /* part 1 of 2 */
	erratum_a009929();
	erratum_a009660();
	erratum_a010539();
}
#endif

#ifdef CONFIG_QSPI_AHB_INIT
/* Enable 4bytes address support and fast read */
int qspi_ahb_init(void)
{
	u32 *qspi_lut, lut_key, *qspi_key;

	qspi_key = (void *)SYS_FSL_QSPI_ADDR + 0x300;
	qspi_lut = (void *)SYS_FSL_QSPI_ADDR + 0x310;

	lut_key = in_be32(qspi_key);

	if (lut_key == 0x5af05af0) {
		/* That means the register is BE */
		out_be32(qspi_key, 0x5af05af0);
		/* Unlock the lut table */
		out_be32(qspi_key + 1, 0x00000002);
		out_be32(qspi_lut, 0x0820040c);
		out_be32(qspi_lut + 1, 0x1c080c08);
		out_be32(qspi_lut + 2, 0x00002400);
		/* Lock the lut table */
		out_be32(qspi_key, 0x5af05af0);
		out_be32(qspi_key + 1, 0x00000001);
	} else {
		/* That means the register is LE */
		out_le32(qspi_key, 0x5af05af0);
		/* Unlock the lut table */
		out_le32(qspi_key + 1, 0x00000002);
		out_le32(qspi_lut, 0x0820040c);
		out_le32(qspi_lut + 1, 0x1c080c08);
		out_le32(qspi_lut + 2, 0x00002400);
		/* Lock the lut table */
		out_le32(qspi_key, 0x5af05af0);
		out_le32(qspi_key + 1, 0x00000001);
	}

	return 0;
}
#endif

#ifdef CONFIG_BOARD_LATE_INIT
int board_late_init(void)
{
#ifdef CONFIG_SCSI_AHCI_PLAT
	sata_init();
#endif
#ifdef CONFIG_CHAIN_OF_TRUST
	fsl_setenv_chain_of_trust();
#endif
#ifdef CONFIG_QSPI_AHB_INIT
	qspi_ahb_init();
#endif

	return 0;
}
#endif
