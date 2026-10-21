-- Validate correct lib version shows
SHOW supautils.version;

-- Validate that user is not able to change version
SET supautils.version to 'v0.0.0';

-- Validate the deprecated parameter doesnt show up on pg_settings
SELECT name FROM pg_settings WHERE name = 'supautils.version';
