-- QK Chat Database Status Check Script

USE qkchat;

-- Check friendship status
SELECT '=== Friendship Status ===' as info;
SELECT 
    status,
    COUNT(*) as count
FROM friendships 
GROUP BY status
ORDER BY status;

-- Check friend request status
SELECT '=== Friend Request Status ===' as info;
SELECT 
    CONCAT(requester_status, '->', target_status) as status_flow,
    COUNT(*) as count
FROM friend_requests 
GROUP BY requester_status, target_status
ORDER BY requester_status, target_status;

-- Check for deleted status friendships
SELECT '=== Check Deleted Status Friendships ===' as info;
SELECT COUNT(*) as deleted_count FROM friendships WHERE status = 'deleted';

-- Check for processed friend requests
SELECT '=== Check Processed Friend Requests ===' as info;
SELECT COUNT(*) as processed_count FROM friend_requests 
WHERE (requester_status = 'accepted' AND target_status = 'accepted') 
   OR (requester_status = 'rejected' AND target_status = 'rejected');
