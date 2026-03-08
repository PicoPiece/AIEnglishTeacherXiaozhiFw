DESCRIBE ai_agent_chat_history;
DESCRIBE ai_device;
DESCRIBE sys_user;
SELECT COUNT(*) as chat_count FROM ai_agent_chat_history;
SELECT COUNT(*) as device_count FROM ai_device;
SELECT id,username FROM sys_user;
