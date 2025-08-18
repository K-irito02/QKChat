-- QK Chat 最小化数据清理脚本
-- 只处理核心问题，避免字符集冲突

USE qkchat;

-- 1. 清理所有deleted状态的好友关系记录
DELETE FROM friendships WHERE status = 'deleted';
SELECT 'Deleted friendships cleaned' as result;

-- 2. 清理所有已处理的好友请求记录
DELETE FROM friend_requests 
WHERE (requester_status = 'accepted' AND target_status = 'accepted') 
   OR (requester_status = 'rejected' AND target_status = 'rejected');
SELECT 'Processed friend requests cleaned' as result;

-- 3. 清理孤立的好友请求通知记录
DELETE frn FROM friend_request_notifications frn
LEFT JOIN friend_requests fr ON frn.request_id = fr.id
WHERE fr.id IS NULL;
SELECT 'Orphaned notifications cleaned' as result;

-- 显示清理后的状态
SELECT '=== after cleanup ===' as info;
SELECT 'friendships' as table_name, status, COUNT(*) as count FROM friendships GROUP BY status
UNION ALL
SELECT 'friend_requests' as table_name, CONCAT(requester_status, '->', target_status) as status, COUNT(*) as count 
FROM friend_requests GROUP BY requester_status, target_status
ORDER BY table_name, status;
