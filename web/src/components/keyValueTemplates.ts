// Factory templates for SettingsKeyValueCollection. Selecting one prefills
// the Add dialog with a suggested key and value; the user can still edit
// both before confirming.
export interface KeyValueTemplate {
    // Menu label.
    label: string;
    // Optional secondary text shown in the menu.
    description?: string;
    // Suggested key (Add dialog will dedupe with _2/_3 suffixes on collision).
    defaultKey: string;
    // Initial value.
    defaultValue: string;
}

const CHECK_HELPERS_ALIASES: KeyValueTemplate[] = [
    {
        label: "alias_cpu_bounded",
        description: "CPU load with bounds",
        defaultKey: "alias_cpu_bounded",
        defaultValue: 'check_cpu "warn=load > 80%" "crit=load > 90%"',
    },
    {
        label: "alias_mem_bounded",
        description: "Memory check with bounds.",
        defaultKey: "alias_mem_bounded",
        defaultValue: 'check_memory "warn=used>80%" "crit=used>90%"'
    },
    {
        label: "alias_top_memory_users",
        description: "Top 5 processes by working set (3G/4G).",
        defaultKey: "alias_top_memory_users",
        defaultValue:
            'filter_perf sort=normal limit=5 command=check_process arguments "warn=working_set > 3G" "crit=working_set > 4G" "detail-syntax=%(exe) ws=%(working_set)"',
    },
    {
        label: "alias_service_excluded",
        description: "Service status excluding known noisy drivers.",
        defaultKey: "alias_service_excluded",
        defaultValue: 'check_service "exclude=Net Driver HPZ12" "exclude=Pml Driver HPZ12" exclude=stisvc',
    },
    {
        label: "alias_process_started",
        description: "Process by name — alert if not started.",
        defaultKey: "alias_process_started",
        defaultValue: 'check_process "process=notepad.exe" "crit=state != \'started\'"',
    },
    {
        label: "alias_process_stopped",
        description: "Process by name — alert if not stopped.",
        defaultKey: "alias_process_stopped",
        defaultValue: 'check_process "process=$ARG1$" "crit=state != \'stopped\'"',
    },
    {
        label: "alias_process_count",
        description: "Process count with thresholds.",
        defaultKey: "alias_process_count",
        defaultValue: 'check_process "process=notepad.exe" "warn=count > 1" "crit=count > 5"',
    },
    {
        label: "alias_process_hung",
        description: "Alert when any process is hung.",
        defaultKey: "alias_process_hung",
        defaultValue: 'check_process "filter=is_hung" "crit=count>0"',
    },
    {
        label: "alias_file_size",
        description: "File size threshold (path=$ARG1$, crit=$ARG2$).",
        defaultKey: "alias_file_size",
        defaultValue:
            'check_files "path=$ARG1$" "crit=size > $ARG2$" "top-syntax=${list}" "detail-syntax=${filename] ${size}" max-dir-depth=10',
    },
    {
        label: "alias_file_age",
        description: "File age threshold (path=$ARG1$, crit=$ARG2$).",
        defaultKey: "alias_file_age",
        defaultValue:
            'check_files "path=$ARG1$" "crit=written > $ARG2$" "top-syntax=${list}" "detail-syntax=${filename] ${written}" max-dir-depth=10',
    },
    {
        label: "alias_sched_all",
        description: "All scheduled tasks; crit on non-zero exit code.",
        defaultKey: "alias_sched_all",
        defaultValue: 'check_tasksched show-all "syntax=${title}: ${exit_code}" "crit=exit_code ne 0"',
    },
    {
        label: "alias_sched_long",
        description: "Long-running scheduled tasks ($ARG1$ threshold).",
        defaultKey: "alias_sched_long",
        defaultValue:
            'check_tasksched "filter=status = \'running\'" "detail-syntax=${title} (${most_recent_run_time})" "crit=most_recent_run_time < -$ARG1$"',
    },
    {
        label: "alias_sched_task",
        description: "Specific scheduled task by title ($ARG1$).",
        defaultKey: "alias_sched_task",
        defaultValue:
            'check_tasksched show-all "filter=title eq \'$ARG1$\'" "detail-syntax=${title} (${exit_code})" "crit=exit_code ne 0"',
    },
];

export const KEY_VALUE_TEMPLATES: Record<string, KeyValueTemplate[]> = {
    "/settings/check helpers/alias": CHECK_HELPERS_ALIASES,
};

export function getKeyValueTemplates(collectionPath: string): KeyValueTemplate[] {
    return KEY_VALUE_TEMPLATES[collectionPath] ?? [];
}
