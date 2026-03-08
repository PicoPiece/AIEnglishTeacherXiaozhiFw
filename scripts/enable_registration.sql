UPDATE sys_params SET param_value='true' WHERE param_code='server.allow_user_register';
SELECT param_code,param_value FROM sys_params WHERE param_code='server.allow_user_register';
