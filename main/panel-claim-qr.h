#ifndef __PANEL_CLAIM_QR_H__
#define __PANEL_CLAIM_QR_H__

// Push a full-screen panel rendering %%uri%% as a QR code for the phone to scan.
// The pointer is copied, so the caller's buffer need not outlive the call.
int pushPanelClaimQR(const char *uri);

#endif /* __PANEL_CLAIM_QR_H__ */
