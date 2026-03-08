SELECT id, user_id, mac_address, board, alias, agent_id, app_version FROM ai_device;
SELECT param_code, param_value FROM sys_params WHERE param_code IN ('server.allow_user_register','server.enable_mobile_register');
