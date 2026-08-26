use zed_extension_api as zed;

struct MyExtension;

impl MyExtension {
    fn get_path_to_language_server_executable(&self) -> zed::Result<String> {
        Ok("D:\\tmpLang\\build\\debug\\qVi-lsp.exe".to_string())
    }

    fn get_args_for_language_server(&self) -> zed::Result<Vec<String>> {
        Ok(vec![])
    }

    fn get_env_for_language_server(&self) -> zed::Result<Vec<(String, String)>> {
        Ok(vec![])
    }
}

impl zed::Extension for MyExtension {
    fn new() -> Self {
       Self
    }

    fn language_server_command(
        &mut self,
        language_server_id: &zed::LanguageServerId,
        worktree: &zed::Worktree,
    ) -> zed::Result<zed::Command> {
        Ok(zed::Command {
            command: self.get_path_to_language_server_executable()?,
            args: self.get_args_for_language_server()?,
            env: self.get_env_for_language_server()?,
        })
    }
}

zed::register_extension!(MyExtension);
