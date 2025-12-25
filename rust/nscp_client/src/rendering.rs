use crate::cli::{OutputFormat, OutputStyle};
use indexmap::IndexMap;
use serde::Serialize;
use tabled::settings::location::ByColumnName;
use tabled::settings::{Remove, Style};
use tabled::{Table, Tabled};

pub trait RenderToString {
    fn render(&self, str: &str);
}

pub struct PrintRender {}

impl PrintRender {
    pub(crate) fn new() -> Self {
        Self {}
    }
}

impl RenderToString for PrintRender {
    fn render(&self, str: &str) {
        println!("{}", str.trim_end());
    }
}

pub struct Rendering {
    output: OutputFormat,
    output_long: bool,
    output_style: OutputStyle,
    sink: Box<dyn RenderToString>,
}

impl Rendering {
    pub fn new(
        output: OutputFormat,
        output_style: OutputStyle,
        output_long: bool,
        sink: Box<dyn RenderToString>,
    ) -> Self {
        Self {
            output,
            output_long,
            output_style,
            sink,
        }
    }

    pub fn is_text(&self) -> bool {
        matches!(self.output, OutputFormat::Text)
    }

    pub fn is_flat(&self) -> bool {
        matches!(self.output, OutputFormat::Text | OutputFormat::Csv)
    }

    fn configure_table_no_headers(&self, table: &mut Table) {
        match self.output_style {
            OutputStyle::Blank => table.with(Style::blank()),
            OutputStyle::Markdown => table.with(Style::markdown().remove_horizontals()),
            OutputStyle::Rounded => table.with(Style::rounded().remove_horizontals()),
        };
    }
    fn configure_table(&self, table: &mut Table) {
        match self.output_style {
            OutputStyle::Blank => table.with(Style::blank()),
            OutputStyle::Markdown => table.with(Style::markdown()),
            OutputStyle::Rounded => table.with(Style::rounded()),
        };
    }

    pub fn render_nested_list<T: Serialize>(&self, object: &[T]) -> anyhow::Result<()> {
        match self.output {
            OutputFormat::Text => anyhow::bail!("Nested not supported in text output"),
            OutputFormat::Csv => anyhow::bail!("Nested not supported in csv output"),
            OutputFormat::Json => {
                let output = serde_json::to_string_pretty(object)?;
                self.sink.render(&output);
            }
            OutputFormat::Yaml => {
                let output = serde_yaml::to_string(object)?;
                self.sink.render(&output);
            }
        }
        Ok(())
    }

    pub fn render_flat_list<T: Tabled + Serialize>(
        &self,
        object: &[T],
        long: &bool,
        long_columns: &[&str],
    ) -> anyhow::Result<()> {
        match self.output {
            OutputFormat::Json => anyhow::bail!("Flat not supported in Json output"),
            OutputFormat::Yaml => anyhow::bail!("Flat not supported in Yaml output"),
            OutputFormat::Text => {
                let mut table = Table::new(object);
                self.configure_table(&mut table);
                if !self.output_long && !long {
                    for col in long_columns {
                        table.with(Remove::column(ByColumnName::new(*col)));
                    }
                }
                self.sink.render(table.to_string().as_str());
            }
            OutputFormat::Csv => {
                let mut wtr = csv::Writer::from_writer(std::io::stdout());
                for record in object {
                    wtr.serialize(record)?;
                }
                wtr.flush()?;
            }
        }
        Ok(())
    }

    pub fn render_nested_single<T: Serialize>(&self, object: &T) -> anyhow::Result<()> {
        match self.output {
            OutputFormat::Text => anyhow::bail!("Nested not supported in text output"),
            OutputFormat::Csv => anyhow::bail!("Nested not supported in csv output"),
            OutputFormat::Json => {
                let output = serde_json::to_string_pretty(object)?;
                self.sink.render(&output);
            }
            OutputFormat::Yaml => {
                let output = serde_yaml::to_string(object)?;
                self.sink.render(&output);
            }
        }
        Ok(())
    }

    pub fn render_flat_single(&self, object: &IndexMap<String, String>) -> anyhow::Result<()> {
        match self.output {
            OutputFormat::Json => anyhow::bail!("Flat not supported in Json output"),
            OutputFormat::Yaml => anyhow::bail!("Flat not supported in Yaml output"),
            OutputFormat::Text => {
                let mut table = Table::nohead(object);
                self.configure_table_no_headers(&mut table);
                self.sink.render(table.to_string().as_str());
            }
            OutputFormat::Csv => {
                let mut wtr = csv::Writer::from_writer(vec![]);
                for record in object {
                    wtr.serialize(record)?;
                }
                wtr.flush()?;
                let result = String::from_utf8(wtr.into_inner()?)?;
                self.sink.render(&result);
            }
        }
        Ok(())
    }

    pub(crate) fn print(&self, text: &str) {
        self.sink.render(text);
    }
}
