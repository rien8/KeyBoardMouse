#include "ui.h"
#include "app.h"
#include "engine.h"
#include "hook.h"

#include "imgui.h"

#include <cstdio>
#include <cstring>

namespace {

void appendUtf8(char* buf, size_t cap, const char* text)
{
    size_t len = strlen(buf);
    snprintf(buf + len, cap - len, "%s", text);
}

void maskText(unsigned mask, char* out, size_t cap)
{
    out[0] = '\0';
    if (mask & DIR_UP) appendUtf8(out, cap, u8"上");
    if (mask & DIR_DOWN) appendUtf8(out, cap, u8"下");
    if (mask & DIR_LEFT) appendUtf8(out, cap, u8"左");
    if (mask & DIR_RIGHT) appendUtf8(out, cap, u8"右");
    if (mask & WHEEL_UP) appendUtf8(out, cap, u8"滚上");
    if (mask & WHEEL_DOWN) appendUtf8(out, cap, u8"滚下");
    if (out[0] == '\0')
        snprintf(out, cap, u8"—");
}

void hint(const char* text)
{
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("%s", text);
}

} // namespace

void ui::draw(App& app, UiState& st)
{
    Config& cfg = app.cfg();

    ImGui::SetNextWindowSize(ImVec2(540, 600), ImGuiCond_FirstUseEver);
    ImGui::Begin(u8"鼠标键", nullptr, ImGuiWindowFlags_NoCollapse);

    bool enabled = cfg.enabled;
    if (ImGui::Checkbox(u8"启用", &enabled)) {
        app.setEnabled(enabled);
        ImGui::End();
        return;
    }

    unsigned mask = 0;
    double speed = 0.0;
    app.engine().status(mask, speed);
    char maskBuf[64];
    maskText(mask, maskBuf, sizeof(maskBuf));
    ImGui::SameLine();
    ImGui::TextDisabled(u8"输入: %s  速度: %.0f px/s", maskBuf, speed);

    ImGui::Separator();

    if (ImGui::CollapsingHeader(u8"运动参数", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::DragFloat(u8"起步速度 px/s", &cfg.motion.vStart, 1.0f, 1.0f, 2000.0f, "%.0f");
        hint(u8"按下瞬间的初速,越小起手越稳。");
        ImGui::DragFloat(u8"最大速度 px/s", &cfg.motion.vMax, 10.0f, 100.0f, 8000.0f, "%.0f");
        hint(u8"满速,决定跨屏耗时。");
        ImGui::DragFloat(u8"加速时间 ms", &cfg.motion.rampMs, 5.0f, 1.0f, 5000.0f, "%.0f");
        hint(u8"从起步速度爬到最大速度的时间。");
        ImGui::SliderFloat(u8"曲线弯曲 gamma", &cfg.motion.gamma, 0.1f, 4.0f, "%.2f");
        hint(u8"小于 1 起手快,大于 1 起手慢、低速段长。");
        ImGui::SliderFloat(u8"斜向系数", &cfg.motion.diagScale, 0.5f, 1.0f, "%.3f");
        hint(u8"0.707 时斜向与直向同速(各向同性)。");
        ImGui::DragFloat(u8"滚轮速度 格/s", &cfg.motion.wheelRate, 1.0f, 1.0f, 60.0f, "%.0f");
    }

    if (ImGui::CollapsingHeader(u8"按键映射", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH;
        if (ImGui::BeginTable("bindings", 3, flags)) {
            ImGui::TableSetupColumn(u8"动作", ImGuiTableColumnFlags_WidthFixed, 110.0f);
            ImGui::TableSetupColumn(u8"按键", ImGuiTableColumnFlags_WidthFixed, 130.0f);
            ImGui::TableSetupColumn(u8"改键");
            ImGui::TableHeadersRow();

            for (int i = 1; i < kActionCount; ++i) {
                Action a = (Action)i;
                ImGui::PushID(i);
                ImGui::TableNextRow();

                ImGui::TableNextColumn();
                ImGui::TextUnformatted(actionName(a));

                ImGui::TableNextColumn();
                const unsigned short scan = cfg.bindings[i].scan;
                if (scan)
                    ImGui::TextUnformatted(scanName(scan));
                else
                    ImGui::TextDisabled(u8"未绑定");

                ImGui::TableNextColumn();
                if (st.captureAction == i) {
                    ImGui::Text(u8"按任意键… (Esc 取消)");
                } else if (ImGui::SmallButton(u8"捕获")) {
                    st.captureAction = i;
                    st.conflictAction = 0;
                    app.hook().setCaptureMode(true);
                }
                ImGui::PopID();
            }
            ImGui::EndTable();
        }

        if (st.captureAction != 0)
            ImGui::Text(u8"正在为「%s」捕获按键 — 按任意键确认,Esc 取消",
                       actionName((Action)st.captureAction));

        if (st.conflictAction != 0)
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f),
                               u8"冲突:%s 已绑定「%s」,先解绑再分配",
                               scanName(st.conflictScan),
                               actionName((Action)st.conflictAction));
    }

    if (ImGui::CollapsingHeader(u8"调试")) {
        ImGui::Checkbox(u8"记录 tick CSV (重启后生效)", &cfg.csvLog);
        ImGui::TextDisabled(u8"文件: mousekey_ticks.csv (exe 旁)");
    }

    ImGui::Separator();

    if (ImGui::Button(u8"保存配置"))
        app.saveNow();
    ImGui::SameLine();
    if (ImGui::Button(u8"恢复默认")) {
        cfg = defaultConfig();
        app.applyLive();
        app.saveNow();
    }
    ImGui::SameLine();
    if (ImGui::Button(u8"退出程序"))
        app.quit();

    if (st.savedFlash > 0.0f) {
        st.savedFlash -= ImGui::GetIO().DeltaTime;
        ImGui::SameLine();
        ImGui::Text(u8"已保存 ✓");
    }

    ImGui::Spacing();
    ImGui::TextDisabled(u8"关闭窗口即最小化到托盘;右键托盘图标可开关/退出。");
    ImGui::TextDisabled(u8"配置文件: %s", app.iniPathUtf8());

    ImGui::End();
}
