# Pair Code Bitmap Display Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Show the six-digit HTTP pairing code on the two GC9D01 eye LCDs during AP provisioning without enabling LVGL.

**Architecture:** Keep display ownership inside `bk_dual_screen_avi_player`. Add a small RGB565 bitmap digit renderer that stops AVI while provisioning code is visible, renders the first three digits on LCD0 and the last three digits on LCD1, then lets existing AVI start/stop paths resume normal eyes after binding.

**Tech Stack:** BK7258 Armino display API, dual SPI LCD controller, RGB565 framebuffer, static 5x7 bitmap digit font.

---

### Task 1: Static Guard

**Files:**
- Modify: `projects/beken_agent_http/tools/check_http_ai_product_boundary.py`

- [ ] **Step 1: Add guard rules**

Require `Device_Api_Request_Pair_Code()` to call `bk_pair_code_display_show(out->code)` after a successful pair-code response, and require the display implementation to avoid `lvgl` includes.

- [ ] **Step 2: Run guard before implementation**

Run: `python3 projects/beken_agent_http/tools/check_http_ai_product_boundary.py`

Expected: FAIL because `bk_pair_code_display_show` is not implemented or not called.

### Task 2: Bitmap Display Module

**Files:**
- Create: `components/bk_dual_screen_avi_player/bk_pair_code_display.c`
- Modify: `components/bk_dual_screen_avi_player/bk_dual_screen_avi_player.h`
- Modify: `components/bk_dual_screen_avi_player/CMakeLists.txt`

- [ ] **Step 1: Implement renderer**

Create a product-only renderer with API:

```c
bk_err_t bk_pair_code_display_show(const char *six_digit_code);
bk_err_t bk_pair_code_display_clear(void);
```

It validates exactly six ASCII digits, opens the dual SPI display using the existing `dual_spi_ctlr_config`, draws `123` and `456` into a 160x160 RGB565 framebuffer, flushes once, and releases display resources on clear.

- [ ] **Step 2: Keep AVI ownership safe**

`bk_pair_code_display_show()` stops AVI first via `bk_dual_screen_avi_player_stop()` so AVI decode and static pair-code display do not flush concurrently.

### Task 3: Pair Code Hook

**Files:**
- Modify: `projects/beken_agent_http/ap/device_api_client.c`
- Modify: `projects/beken_agent_http/ap/CMakeLists.txt`

- [ ] **Step 1: Hook success path**

After `[HTTP_DEVICE_API] PAIR_CODE=...` is logged, call:

```c
(void)bk_pair_code_display_show(out->code);
```

- [ ] **Step 2: Clear on binding success and token clear**

When a device token is saved or cleared, call `bk_pair_code_display_clear()` so the display does not keep stale digits.

### Task 4: Verification

**Files:**
- Modify: generated build output only

- [ ] **Step 1: Run static guard**

Run: `python3 projects/beken_agent_http/tools/check_http_ai_product_boundary.py`

Expected: PASS.

- [ ] **Step 2: Build**

Run: `tools/build_beken_agent_http.sh --no-apply-patches`

Expected: build exits 0 and generates `all-app.bin`.

- [ ] **Step 3: Copy firmware**

Copy `all-app.bin` and `app_pack.rbl` to the Windows burn tool folder with timestamped names.
