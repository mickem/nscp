export interface SettingsKey {
    title: string;
    name: string;
    description: string;
    metadata: Map<string, string>;
    plugin: string;
    value: string;
}

export interface Settings {
    [key: string]: SettingsKey;
}

export interface SettingsDesc {
    default_value: string;
    description: string;
    icon: string;
    is_advanced_key: boolean;
    is_object: boolean;
    is_sample_key: string;
    is_template_key: string;
    key: string;
    path: string;
    plugins: string;
    sample_usage: string;
    title: string;
    value: string;
}
