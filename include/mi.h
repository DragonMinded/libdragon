/**
 * @file mi.h
 * @ingroup lowlevel
 * @brief MI register definitions
 */

#ifndef __LIBDRAGON_MI_H
#define __LIBDRAGON_MI_H

#include <stdint.h>

#define MI_MODE                     ((volatile uint32_t*)0xA4300000)    ///< MI_MODE register
#define MI_VERSION                  ((volatile uint32_t*)0xA4300004)    ///< MI_VERSION register
#define MI_INTERRUPT                ((volatile uint32_t*)0xA4300008)    ///< MI_INTERRUPT register
#define MI_MASK                     ((volatile uint32_t*)0xA430000C)    ///< MI_MASK register
#define MI_BB_SECURE_EXCETPION      ((volatile uint32_t*)0xA4300014)    ///< MI_BB_SECURE_EXCETPION register (iQue)
#define MI_BB_RANDOM                ((volatile uint32_t*)0xA430002C)    ///< MI_BB_RANDOM register (iQue)
#define MI_BB_INTERRUPT             ((volatile uint32_t*)0xA4300038)    ///< MI_BB_INTERRUPT register (iQue)
#define MI_BB_MASK                  ((volatile uint32_t*)0xA430003C)    ///< MI_BB_MASK register (iQue)

#define MI_MODE_UPPER               0x00000200      ///< Upper mode (32-bit transfers are shifted in the upper half of the bus)
#define MI_MODE_EBUS                0x00000100      ///< Access to E-Bus (9th bits of RDRAM)
#define MI_MODE_REPEAT              0x00000080      ///< Repeat mode: fast repeated writes of fixed values
#define MI_MODE_REPEAT_COUNT        0x0000007F      ///< Repeat mode: number of times to repeat the value

#define MI_WMODE_SET_UPPER          0x00002000      ///< Enable upper mode
#define MI_WMODE_CLR_UPPER          0x00001000      ///< Disable upper mode
#define MI_WMODE_CLR_DPINT          0x00000800      ///< Clear DP interrupt
#define MI_WMODE_SET_EBUS           0x00000400      ///< Enable E-Bus access mode
#define MI_WMODE_CLR_EBUS           0x00000200      ///< Disable E-Bus access mode
#define MI_WMODE_SET_REPEAT         0x00000100      ///< Enable repeat mode
#define MI_WMODE_CLR_REPEAT         0x00000080      ///< Disable repeat mode

#define MI_BB_INTERRUPT_MD_STATE    0x02000000      ///< 1 if the memory card is present, 0 if missing (iQue)
#define MI_BB_INTERRUPT_BTN_STATE   0x01000000      ///< 1 if the power button is pressed, 0 if not (iQue)
#define MI_BB_INTERRUPT_MD          0x00002000      ///< Pending interrupt: Memory card removed or inserted (iQue)
#define MI_BB_INTERRUPT_BTN         0x00001000      ///< Pending interrupt: Power button pressed (iQue)
#define MI_BB_INTERRUPT_USB1        0x00000800      ///< Pending interrupt: USB1 (iQue)
#define MI_BB_INTERRUPT_USB0        0x00000400      ///< Pending interrupt: USB0 (iQue)
#define MI_BB_INTERRUPT_PI_ERR      0x00000200      ///< Pending interrupt: PI error (iQue)
#define MI_BB_INTERRUPT_IDE         0x00000100      ///< Pending interrupt: IDE (iQue)
#define MI_BB_INTERRUPT_AES         0x00000080      ///< Pending interrupt: AES interrupt (iQue)
#define MI_BB_INTERRUPT_FLASH       0x00000040      ///< Pending interrupt: Flash (iQue)
#define MI_INTERRUPT_DP             0x00000020      ///< Pending interrupt: RDP SYNC_FULL has finished
#define MI_INTERRUPT_PI             0x00000010      ///< Pending interrupt: PI transfer finished
#define MI_INTERRUPT_VI             0x00000008      ///< Pending interrupt: VI line match (normally, vblank)
#define MI_INTERRUPT_AI             0x00000004      ///< Pending interrupt: AI buffer playback started
#define MI_INTERRUPT_SI             0x00000002      ///< Pending interrupt: SI transfer finished
#define MI_INTERRUPT_SP             0x00000001      ///< Pending interrupt: RSP interrupt requested

#define MI_BB_MASK_MD               0x2000          ///< Mask for memory card interrupt (iQue)
#define MI_BB_MASK_BTN              0x1000          ///< Mask for power button interrupt (iQue)
#define MI_BB_MASK_USB1             0x0800          ///< Mask for USB1 interrupt (iQue)
#define MI_BB_MASK_USB0             0x0400          ///< Mask for USB0 interrupt (iQue)
#define MI_BB_MASK_PI_ERR           0x0200          ///< Mask for PI error interrupt (iQue)
#define MI_BB_MASK_IDE              0x0100          ///< Mask for IDE interrupt (iQue)
#define MI_BB_MASK_AES              0x0080          ///< Mask for AES interrupt (iQue)
#define MI_BB_MASK_FLASH            0x0040          ///< Mask for flash interrupt (iQue)
#define MI_MASK_DP                  0x0020          ///< Mask for RDP SYNC_FULL interrupt          
#define MI_MASK_PI                  0x0010          ///< Mask for PI transfer interrupt
#define MI_MASK_VI                  0x0008          ///< Mask for VI line match interrupt        
#define MI_MASK_AI                  0x0004          ///< Mask for AI buffer playback interrupt
#define MI_MASK_SI                  0x0002          ///< Mask for SI transfer interrupt
#define MI_MASK_SP                  0x0001          ///< Mask for RSP interrupt

#define MI_BB_WMASK_SET_MD          0x08000000      ///< Enable memory card interrupt (iQue)
#define MI_BB_WMASK_CLR_MD          0x04000000      ///< Disable memory card interrupt (iQue)
#define MI_BB_WMASK_SET_BTN         0x02000000      ///< Enable power button interrupt (iQue)
#define MI_BB_WMASK_CLR_BTN         0x01000000      ///< Disable power button interrupt (iQue)
#define MI_BB_WMASK_SET_USB1        0x00800000      ///< Enable USB1 interrupt (iQue)
#define MI_BB_WMASK_CLR_USB1        0x00400000      ///< Disable USB1 interrupt (iQue)
#define MI_BB_WMASK_SET_USB0        0x00200000      ///< Enable USB0 interrupt (iQue)
#define MI_BB_WMASK_CLR_USB0        0x00100000      ///< Disable USB0 interrupt (iQue)
#define MI_BB_WMASK_SET_PI_ERR      0x00080000      ///< Enable PI error interrupt (iQue)
#define MI_BB_WMASK_CLR_PI_ERR      0x00040000      ///< Disable PI error interrupt (iQue)
#define MI_BB_WMASK_SET_IDE         0x00020000      ///< Enable IDE interrupt (iQue)
#define MI_BB_WMASK_CLR_IDE         0x00010000      ///< Disable IDE interrupt (iQue)
#define MI_BB_WMASK_SET_AES         0x00008000      ///< Enable AES interrupt (iQue)
#define MI_BB_WMASK_CLR_AES         0x00004000      ///< Disable AES interrupt (iQue)
#define MI_BB_WMASK_SET_FLASH       0x00002000      ///< Enable flash interrupt (iQue)
#define MI_BB_WMASK_CLR_FLASH       0x00001000      ///< Disable flash interrupt (iQue)
#define MI_WMASK_SET_DP             0x00000800      ///< Enable RDP SYNC_FULL interrupt
#define MI_WMASK_CLR_DP             0x00000400      ///< Disable RDP SYNC_FULL interrupt
#define MI_WMASK_SET_PI             0x00000200      ///< Enable PI transfer interrupt
#define MI_WMASK_CLR_PI             0x00000100      ///< Disable PI transfer interrupt
#define MI_WMASK_SET_VI             0x00000080      ///< Enable VI line match interrupt
#define MI_WMASK_CLR_VI             0x00000040      ///< Disable VI line match interrupt
#define MI_WMASK_SET_AI             0x00000020      ///< Enable AI buffer playback interrupt
#define MI_WMASK_CLR_AI             0x00000010      ///< Disable AI buffer playback interrupt
#define MI_WMASK_SET_SI             0x00000008      ///< Enable SI transfer interrupt
#define MI_WMASK_CLR_SI             0x00000004      ///< Disable SI transfer interrupt
#define MI_WMASK_SET_SP             0x00000002      ///< Enable RSP interrupt
#define MI_WMASK_CLR_SP             0x00000001      ///< Disable RSP interrupt

#endif
