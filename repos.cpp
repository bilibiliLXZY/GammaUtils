#include <Windows.h>
#include <wingdi.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <sddl.h>    // 用于权限检查
#include <winsvc.h>  // 用于服务操作

// 检查是否以管理员身份运行
BOOL IsRunningAsAdmin() {
    BOOL isAdmin = FALSE;
    HANDLE hToken = NULL;
    
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) {
        TOKEN_ELEVATION elevation;
        DWORD size = sizeof(TOKEN_ELEVATION);
        
        if (GetTokenInformation(hToken, TokenElevation, &elevation, size, &size)) {
            isAdmin = elevation.TokenIsElevated;
        }
        
        CloseHandle(hToken);
    }
    
    return isAdmin;
}

// 重新以管理员身份启动程序
BOOL RestartAsAdmin() {
		char szPath[MAX_PATH];
		GetModuleFileNameA(NULL, szPath, MAX_PATH);

		SHELLEXECUTEINFO sei = { sizeof(SHELLEXECUTEINFO) };
		sei.lpVerb = "runas";
		sei.lpFile = szPath;
		sei.nShow = SW_HIDE;

		ShellExecuteEx(&sei);
    
    return FALSE;
}

// 增强的伽马调整函数
BOOL AdjustGamma(float gamma, float brightness, float contrast) {
    HDC hDC = GetDC(NULL);
    if (!hDC) {
        printf("错误：无法获取设备上下文 (错误代码: %d)\n", GetLastError());
        return FALSE;
    }
    
    // 检查显卡是否支持伽马调整
    int capabilities = GetDeviceCaps(hDC, RASTERCAPS);
    if (!(capabilities & RC_PALETTE)) {
        printf("警告：当前显示设备可能不支持硬件伽马调整\n");
        // 继续尝试，有些显卡可能仍然支持
    }
    
    // 获取当前伽马表
    WORD GammaArray[3][256];
    
    if (!GetDeviceGammaRamp(hDC, GammaArray)) {
        DWORD error = GetLastError();
        printf("错误：无法获取伽马表 (错误代码: %d)\n", error);
        
        // 提供具体错误信息
        switch (error) {
            case ERROR_ACCESS_DENIED:
                printf("  需要管理员权限才能访问显示设备\n");
                printf("  请右键点击程序，选择'以管理员身份运行'\n");
                break;
            case ERROR_INVALID_HANDLE:
                printf("  无效的设备上下文\n");
                break;
            case ERROR_NOT_SUPPORTED:
                printf("  当前显卡/驱动不支持伽马调整\n");
                break;
            default:
                printf("  未知错误，请检查显卡驱动是否正常\n");
                break;
        }
        
        ReleaseDC(NULL, hDC);
        return FALSE;
    }
    
    // 保存原始设置（如果是第一次）
    static BOOL firstRun = TRUE;
    static WORD OriginalGammaArray[3][256];
    
    if (firstRun) {
        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < 256; j++) {
                OriginalGammaArray[i][j] = GammaArray[i][j];
            }
        }
        firstRun = FALSE;
    }
    
    // 修改伽马表
    printf("正在计算新的伽马表...\n");
    for (int i = 0; i < 256; i++) {
        float normalized = i / 255.0f;
        
        // 应用亮度调整
        normalized += brightness;
        
        // 应用对比度调整
        normalized = (normalized - 0.5f) * contrast + 0.5f;
        
        // 应用伽马校正
        normalized = powf(normalized, gamma);
        
        // 钳制到有效范围
        if (normalized < 0.0f) normalized = 0.0f;
        if (normalized > 1.0f) normalized = 1.0f;
        
        WORD value = (WORD)(normalized * 65535.0f);
        
        GammaArray[0][i] = value; // Red
        GammaArray[1][i] = value; // Green
        GammaArray[2][i] = value; // Blue
    }
    
    // 设置新的伽马表
    printf("正在应用伽马设置...\n");
    
    // 尝试多次设置，有些显卡需要重试
    BOOL result = FALSE;
    int retryCount = 3;
    
    for (int i = 0; i < retryCount && !result; i++) {
        if (i > 0) {
            printf("  第 %d 次重试...\n", i + 1);
            Sleep(100);  // 短暂延迟
        }
        
        result = SetDeviceGammaRamp(hDC, GammaArray);
        
        if (!result) {
            DWORD error = GetLastError();
            
            // 如果是访问被拒绝，可能是权限问题
            if (error == ERROR_ACCESS_DENIED) {
                if (i == retryCount - 1) {  // 最后一次重试失败
                    printf("  权限不足，无法修改显示设置\n");
                    printf("  请确保：\n");
                    printf("  1. 以管理员身份运行此程序\n");
                    printf("  2. 显卡驱动已正确安装\n");
                    printf("  3. 没有其他程序正在修改显示设置\n");
                }
            }
        }
    }
    
    if (!result) {
        printf("? 伽马设置应用失败 (错误代码: %d)\n", GetLastError());
        
        // 尝试恢复原始设置
        SetDeviceGammaRamp(hDC, OriginalGammaArray);
    } else {
        printf("? 伽马设置应用成功\n");
        
        // 显示实际应用的参数
        printf("  实际参数: Gamma=%.2f, Brightness=%.2f, Contrast=%.2f\n", 
               gamma, brightness, contrast);
    }
    
    ReleaseDC(NULL, hDC);
    return result;
}

// 主函数
int main(int argc, char* argv[]) {
    printf("========== 屏幕伽马调整工具 ==========\n");
    printf("版本: 2.0 (增强权限版)\n\n");
    
    // 检查是否以管理员身份运行
    if (!IsRunningAsAdmin()) {
        printf("警告：当前未以管理员身份运行\n");
        printf("伽马调整需要管理员权限才能正常工作\n\n");
        
        printf("是否以管理员身份重新启动？(Y/N): ");
        char choice = getchar();
        
        if (choice == 'Y' || choice == 'y') {
            printf("正在以管理员身份重新启动...\n");
            
            if (RestartAsAdmin()) {
                printf("请在新窗口中继续操作\n");
                return 0;  // 当前进程退出
            } else {
                printf("重新启动失败，请手动以管理员身份运行\n");
                printf("右键点击程序 → '以管理员身份运行'\n\n");
            }
        } else {
            printf("继续以普通权限运行，部分功能可能受限\n\n");
        }
        
        // 清除输入缓冲区
        while (getchar() != '\n');
    } else {
        printf("? 已以管理员身份运行\n\n");
    }
    
    // 检查命令行参数
    if (argc == 4) {
        // 从命令行参数获取设置
        float gamma = atof(argv[1]);
        float brightness = atof(argv[2]);
        float contrast = atof(argv[3]);
        
        printf("从命令行应用设置...\n");
        printf("Gamma: %.2f, Brightness: %.2f, Contrast: %.2f\n", 
               gamma, brightness, contrast);
        
        if (AdjustGamma(gamma, brightness, contrast)) {
            printf("设置成功应用，程序将保持运行...\n");
            printf("按任意键恢复原始设置并退出...\n");
            getchar();
            
            // 恢复原始设置
            AdjustGamma(1.0f, 0.0f, 1.0f);
        }
        
        return 0;
    }
    
    // 交互模式（原有代码，略作修改）
    float gamma = 1.0f;
    float brightness = 0.0f;
    float contrast = 1.0f;
    int choice;
    
    // ... 原有菜单代码 ...
    
    // 简化版菜单
    while (1) {
        printf("\n当前设置: Gamma=%.2f, Brightness=%.2f, Contrast=%.2f\n", 
               gamma, brightness, contrast);
        printf("1. 调整设置并应用\n");
        printf("2. 恢复默认设置\n");
        printf("3. 退出程序\n");
        printf("选择 (1-3): ");
        
        if (scanf("%d", &choice) != 1) {
            printf("输入无效\n");
            while (getchar() != '\n');
            continue;
        }
        
        switch (choice) {
            case 1:
                printf("请输入伽马值 (0.0~1.0): ");
                scanf("%f", &gamma);
                printf("请输入亮度值 (-0.5~0.5): ");
                scanf("%f", &brightness);
                printf("请输入对比度 (0.5~2.0): ");
                scanf("%f", &contrast);
                
                // 验证输入范围
                if (gamma < 0.0f) gamma = 0.0f;
                if (gamma > 1.0f) gamma = 1.0f;
                if (brightness < -0.5f) brightness = -0.5f;
                if (brightness > 0.5f) brightness = 0.5f;
                if (contrast < 0.5f) contrast = 0.5f;
                if (contrast > 2.0f) contrast = 2.0f;
                
                AdjustGamma(gamma, brightness, contrast);
                break;
                
            case 2:
                AdjustGamma(1.0f, 0.0f, 1.0f);
                gamma = 1.0f;
                brightness = 0.0f;
                contrast = 1.0f;
                printf("已恢复默认设置\n");
                break;
                
            case 3:
                printf("是否恢复原始设置再退出？(Y/N): ");
                while (getchar() != '\n');  // 清除缓冲区
                choice = getchar();
                
                if (choice == 'Y' || choice == 'y') {
                    AdjustGamma(1.0f, 0.0f, 1.0f);
                }
                
                printf("感谢使用，再见！\n");
                Sleep(1000);
                return 0;
                
            default:
                printf("无效选择\n");
                break;
        }
        
        // 清除输入缓冲区
        while (getchar() != '\n');
    }
}
