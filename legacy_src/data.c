#include "data.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include "../include/cJSON.h"

#ifdef _WIN32
#include <direct.h>
#define mkdir(path, mode) _mkdir(path)
#endif

AppData g_appData;
const char* DATA_FILE_PATH = "data/database.json";

// Helper: Get current date string YYYY-MM-DD
void GetCurrentDateStr(char* buffer) {
    if (g_appData.is_simulating) {
        sprintf(buffer, "%04d-%02d-%02d", g_appData.sim_year, g_appData.sim_month, g_appData.sim_day);
    } else {
        time_t t = time(NULL);
        struct tm tm = *localtime(&t);
        sprintf(buffer, "%04d-%02d-%02d", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);
    }
}

// Helper calculates diff in days between two date strings
int DaysBetween(const char* date1, const char* date2) {
    struct tm tm1 = {0}, tm2 = {0};
    int y, m, d;
    
    sscanf(date1, "%d-%d-%d", &y, &m, &d);
    tm1.tm_year = y - 1900; tm1.tm_mon = m - 1; tm1.tm_mday = d;
    
    sscanf(date2, "%d-%d-%d", &y, &m, &d);
    tm2.tm_year = y - 1900; tm2.tm_mon = m - 1; tm2.tm_mday = d;

    time_t t1 = mktime(&tm1);
    time_t t2 = mktime(&tm2);
    
    double seconds = difftime(t2, t1);
    return (int)(seconds / (60 * 60 * 24));
}

void InitData() {
    memset(&g_appData, 0, sizeof(AppData));
    
    // Init simulation defaults to today
    time_t t = time(NULL);
    struct tm tm = *localtime(&t);
    g_appData.sim_year = tm.tm_year + 1900;
    g_appData.sim_month = tm.tm_mon + 1;
    g_appData.sim_day = tm.tm_mday;
    g_appData.is_simulating = false;
    
    GetCurrentDateStr(g_appData.current_date);
    
    struct stat st = {0};
    if (stat("data", &st) == -1) {
        mkdir("data", 0700);
    }
}

static AppData g_backupData; // Backup for simulation mode sandbox

void SaveData() {
    if (g_appData.is_simulating) return; // PROHIBIT saving during simulation

    cJSON* root = cJSON_CreateObject();
    
    // Do NOT save simulation state, but logic relies on saving persistent data.
    // If we are simulating, we probably still want to save the "Last checked" dates?
    // User didn't specify behavior of saving while in test mode.
    // Usually test mode shouldn't corrupt real data, but here user said "software thinks it is that time".
    // I will assume operations in test mode ARE saved (e.g. user checks a box in future).
    
    cJSON_AddStringToObject(root, "last_date", g_appData.current_date); 
    
    cJSON* linesArray = cJSON_CreateArray();
    for (int i = 0; i < g_appData.line_count; i++) {
        cJSON* lineObj = cJSON_CreateObject();
        cJSON_AddStringToObject(lineObj, "name", g_appData.lines[i].name);
        
        cJSON* eqArray = cJSON_CreateArray();
        for (int j = 0; j < g_appData.lines[i].equipment_count; j++) {
            cJSON* eqObj = cJSON_CreateObject();
            cJSON_AddStringToObject(eqObj, "name", g_appData.lines[i].equipments[j].name);
            cJSON_AddNumberToObject(eqObj, "scroll_offset", g_appData.lines[i].equipments[j].scroll_offset);
            
            cJSON* itemArray = cJSON_CreateArray();
            for (int k = 0; k < g_appData.lines[i].equipments[j].item_count; k++) {
                cJSON* itemObj = cJSON_CreateObject();
                cJSON_AddStringToObject(itemObj, "name", g_appData.lines[i].equipments[j].items[k].name);
                cJSON_AddNumberToObject(itemObj, "period", g_appData.lines[i].equipments[j].items[k].period_days);
                cJSON_AddStringToObject(itemObj, "last_checked", g_appData.lines[i].equipments[j].items[k].last_checked_date);
                cJSON_AddBoolToObject(itemObj, "is_checked", g_appData.lines[i].equipments[j].items[k].is_checked_today);
                cJSON_AddItemToArray(itemArray, itemObj);
            }
            cJSON_AddItemToObject(eqObj, "items", itemArray);
            cJSON_AddItemToArray(eqArray, eqObj);
        }
        cJSON_AddItemToObject(lineObj, "equipments", eqArray);
        cJSON_AddItemToArray(linesArray, lineObj);
    }
    cJSON_AddItemToObject(root, "lines", linesArray);
    
    char* jsonStr = cJSON_Print(root);
    FILE* fp = fopen(DATA_FILE_PATH, "w");
    if (fp) {
        fputs(jsonStr, fp);
        fclose(fp);
    }
    free(jsonStr);
    cJSON_Delete(root);
}

void LoadData() {
    FILE* fp = fopen(DATA_FILE_PATH, "r");
    if (!fp) return;
    
    fseek(fp, 0, SEEK_END);
    long length = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    char* data = (char*)malloc(length + 1);
    fread(data, 1, length, fp);
    data[length] = '\0';
    fclose(fp);
    
    cJSON* root = cJSON_Parse(data);
    if (!root) { free(data); return; }
    
    char today[11];
    GetCurrentDateStr(today);
    
    cJSON* linesArray = cJSON_GetObjectItem(root, "lines");
    g_appData.line_count = 0;
    
    cJSON* lineObj = NULL;
    cJSON_ArrayForEach(lineObj, linesArray) {
        if (g_appData.line_count >= MAX_LINES) break;
        ProductionLine* line = &g_appData.lines[g_appData.line_count++];
        
        cJSON* name = cJSON_GetObjectItem(lineObj, "name");
        if (name) strcpy(line->name, name->valuestring);
        
        cJSON* eqArray = cJSON_GetObjectItem(lineObj, "equipments");
        line->equipment_count = 0;
        cJSON* eqObj = NULL;
        cJSON_ArrayForEach(eqObj, eqArray) {
            if (line->equipment_count >= MAX_EQUIPMENT) break;
            Equipment* eq = &line->equipments[line->equipment_count++];
            
            cJSON* eqName = cJSON_GetObjectItem(eqObj, "name");
            if (eqName) strcpy(eq->name, eqName->valuestring);
            
            // Check if scroll offset saved, else 0
            cJSON* offset = cJSON_GetObjectItem(eqObj, "scroll_offset");
            eq->scroll_offset = offset ? offset->valueint : 0;
            
            cJSON* itemArray = cJSON_GetObjectItem(eqObj, "items");
            eq->item_count = 0;
            cJSON* itemObj = NULL;
            cJSON_ArrayForEach(itemObj, itemArray) {
                 if (eq->item_count >= MAX_ITEMS) break;
                 MaintenanceItem* item = &eq->items[eq->item_count++];
                 
                 cJSON* iName = cJSON_GetObjectItem(itemObj, "name");
                 if (iName) strcpy(item->name, iName->valuestring);
                 
                 cJSON* period = cJSON_GetObjectItem(itemObj, "period");
                 if (period) item->period_days = period->valueint;
                 
                 cJSON* lastChecked = cJSON_GetObjectItem(itemObj, "last_checked");
                 if (lastChecked) strcpy(item->last_checked_date, lastChecked->valuestring);
                 
                 // If last_checked == today (simulated or real), then it is checked.
                 if (strcmp(item->last_checked_date, today) == 0) {
                     item->is_checked_today = true;
                 } else {
                     item->is_checked_today = false;
                 }
            }
        }
    }
    
    strcpy(g_appData.current_date, today);
    cJSON_Delete(root);
    free(data);
}

void AddLine(const char* name) {
    if (g_appData.line_count < MAX_LINES) {
        strcpy(g_appData.lines[g_appData.line_count++].name, name);
        SaveData();
    }
}

void AddEquipment(int lineIndex, const char* name) {
    if (lineIndex < 0 || lineIndex >= g_appData.line_count) return;
    ProductionLine* line = &g_appData.lines[lineIndex];
    if (line->equipment_count < MAX_EQUIPMENT) {
        strcpy(line->equipments[line->equipment_count].name, name);
        line->equipments[line->equipment_count].scroll_offset = 0;
        line->equipment_count++;
        SaveData();
    }
}

void AddMaintenanceItem(int lineIndex, int equipmentIndex, const char* name, int period) {
    if (lineIndex < 0 || lineIndex >= g_appData.line_count) return;
    ProductionLine* line = &g_appData.lines[lineIndex];
    if (equipmentIndex < 0 || equipmentIndex >= line->equipment_count) return;
    Equipment* eq = &line->equipments[equipmentIndex];
    
    if (eq->item_count < MAX_ITEMS) {
        MaintenanceItem* item = &eq->items[eq->item_count++];
        strcpy(item->name, name);
        item->period_days = period;
        
        // Use CURRENT date instead of 1970-01-01
        char today[11];
        GetCurrentDateStr(today);
        strcpy(item->last_checked_date, today); 
        
        item->is_checked_today = false; // Add new item is unchecked even if date set to today? 
        // Logic: if I say "last checked today", then it IS checked? 
        // User said: "新增時會預設為1970... 修改為今天時間"
        // If I set last_checked to today, CheckMaintenanceStatus() logic might mark it checked?
        // Actually `is_checked_today` is explicit.
        // If I set last_checked = today, but is_checked_today = false, then it is NOT CHECKED, but Last Checked Date is fresh.
        // This effectively resets the "Overdue" counter to 0 days. Correct.
        
        SaveData();
    }
}

void CheckMaintenanceStatus() {
    char today[11];
    GetCurrentDateStr(today);
    
    if (strcmp(today, g_appData.current_date) != 0) {
        strcpy(g_appData.current_date, today);
        for (int i = 0; i < g_appData.line_count; i++) {
            for (int j = 0; j < g_appData.lines[i].equipment_count; j++) {
                for (int k = 0; k < g_appData.lines[i].equipments[j].item_count; k++) {
                     MaintenanceItem* item = &g_appData.lines[i].equipments[j].items[k];
                     if (strcmp(item->last_checked_date, today) != 0) {
                         item->is_checked_today = false;
                     } else {
                         item->is_checked_today = true; // If dates match, it's checked
                     }
                }
            }
        }
        SaveData();
    }
}

void SetMaintenanceItemStatus(int lineIndex, int eqIndex, int itemIndex, bool status) {
    if (lineIndex < 0 || lineIndex >= g_appData.line_count) return;
    MaintenanceItem* item = &g_appData.lines[lineIndex].equipments[eqIndex].items[itemIndex];
    
    item->is_checked_today = status;
    if (status) {
        GetCurrentDateStr(item->last_checked_date);
    }
    SaveData();
}

void UpdateLineName(int lineIndex, const char* newName) {
    if (lineIndex >= 0 && lineIndex < g_appData.line_count) {
        strcpy(g_appData.lines[lineIndex].name, newName);
        SaveData();
    }
}

void UpdateEquipmentName(int lineIndex, int eqIndex, const char* newName) {
    if (lineIndex >= 0 && lineIndex < g_appData.line_count) {
        if (eqIndex >= 0 && eqIndex < g_appData.lines[lineIndex].equipment_count) {
            strcpy(g_appData.lines[lineIndex].equipments[eqIndex].name, newName);
            SaveData();
        }
    }
}

void UpdateItemName(int lineIndex, int eqIndex, int itemIndex, const char* newName) {
    if (lineIndex >= 0 && lineIndex < g_appData.line_count) {
        if (eqIndex >= 0 && eqIndex < g_appData.lines[lineIndex].equipment_count) {
            if (itemIndex >= 0 && itemIndex < g_appData.lines[lineIndex].equipments[eqIndex].item_count) {
                strcpy(g_appData.lines[lineIndex].equipments[eqIndex].items[itemIndex].name, newName);
                SaveData();
            }
        }
    }
}

void DuplicateLine(int lineIndex) {
    if (lineIndex >= 0 && lineIndex < g_appData.line_count && g_appData.line_count < MAX_LINES) {
        ProductionLine* src = &g_appData.lines[lineIndex];
        ProductionLine* dst = &g_appData.lines[g_appData.line_count++];
        
        *dst = *src; // Struct copy
        
        char buf[MAX_NAME_LEN];
        sprintf(buf, "%s (Copy)", src->name);
        strcpy(dst->name, buf);
        
        SaveData();
    }
}

// TODO: Implement Deletes if needed, for now placeholders to satify header
void DeleteLine(int lineIndex) {
    if (lineIndex >= 0 && lineIndex < g_appData.line_count) {
        // Shift remaining lines left
        for (int i = lineIndex; i < g_appData.line_count - 1; i++) {
            g_appData.lines[i] = g_appData.lines[i + 1];
        }
        g_appData.line_count--;
        SaveData();
    }
}

void DeleteEquipment(int lineIndex, int eqIndex) {
    if (lineIndex >= 0 && lineIndex < g_appData.line_count) {
        ProductionLine* line = &g_appData.lines[lineIndex];
        if (eqIndex >= 0 && eqIndex < line->equipment_count) {
            // Shift
            for (int i = eqIndex; i < line->equipment_count - 1; i++) {
                line->equipments[i] = line->equipments[i + 1];
            }
            line->equipment_count--;
            SaveData();
        }
    }
}

void DeleteItem(int lineIndex, int eqIndex, int itemIndex) {
    if (lineIndex >= 0 && lineIndex < g_appData.line_count) {
        ProductionLine* line = &g_appData.lines[lineIndex];
        if (eqIndex >= 0 && eqIndex < line->equipment_count) {
            Equipment* eq = &line->equipments[eqIndex];
            if (itemIndex >= 0 && itemIndex < eq->item_count) {
                // Shift
                for (int i = itemIndex; i < eq->item_count - 1; i++) {
                    eq->items[i] = eq->items[i + 1];
                }
                eq->item_count--;
                SaveData();
            }
        }
    }
}


void SetSimulationDate(int year, int month, int day) {
    g_appData.sim_year = year;
    g_appData.sim_month = month;
    g_appData.sim_day = day;
    CheckMaintenanceStatus(); // Trigger status update immediately
}

void ToggleSimulation(bool enable) {
    if (enable && !g_appData.is_simulating) {
        // Entering simulation: backup real data
        g_backupData = g_appData;
    } else if (!enable && g_appData.is_simulating) {
        // Exiting simulation: restore real data, BUT preserve Sim Settings
        int sy = g_appData.sim_year;
        int sm = g_appData.sim_month;
        int sd = g_appData.sim_day;
        
        g_appData = g_backupData;
        
        // Restore sim settings so they don't reset to 2000 or real-time
        g_appData.sim_year = sy;
        g_appData.sim_month = sm;
        g_appData.sim_day = sd;
    }
    g_appData.is_simulating = enable;
    
    // Always re-check status upon transition to ensure UI is consistent
    CheckMaintenanceStatus();
}
