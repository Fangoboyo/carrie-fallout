import { Database } from "bun:sqlite";

const db = new Database("app.db", { create: true });

// Create the users table to store authentication details and the generated API key
db.run(`
  CREATE TABLE IF NOT EXISTS users (
    id TEXT PRIMARY KEY,
    google_id TEXT UNIQUE NOT NULL,
    email TEXT,
    name TEXT,
    picture TEXT,
    access_token TEXT,
    refresh_token TEXT,
    api_key TEXT UNIQUE NOT NULL
  )
`);

// Create a videos table to keep track of uploaded videos so friends can see the feed
db.run(`
  CREATE TABLE IF NOT EXISTS videos (
    id TEXT PRIMARY KEY,
    user_id TEXT NOT NULL,
    google_drive_file_id TEXT NOT NULL,
    title TEXT,
    uploaded_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY(user_id) REFERENCES users(id)
  )
`);

export default db;
