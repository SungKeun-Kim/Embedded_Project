/**
 * @file  menu_screen.h
 * @brief LCD 화면 렌더링 (메뉴 상태별 표시)
 */
#ifndef MENU_SCREEN_H
#define MENU_SCREEN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "menu.h"

/**
 * @brief LCD 화면 갱신 — 변경 시에만 재렌더링
 *        메인 루프에서 호출
 */
void MenuScreen_Refresh(void);

/**
 * @brief 강제 화면 갱신 플래그 설정
 */
void MenuScreen_ForceRefresh(void);

#ifdef __cplusplus
}
#endif

#endif /* MENU_SCREEN_H */
