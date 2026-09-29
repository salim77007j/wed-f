#!/usr/bin/env node
/* assistant.js — WED AI sidecar.
 * Reads the user prompt + page context from stdin, calls the GLM model via
 * z-ai-web-dev-sdk, prints the reply to stdout.
 * Usage: node assistant.js <model> < prompt.txt
 */
const fs = require('fs');
const path = require('path');

async function main() {
  const model = process.argv[2] || 'glm-4-flash';
  const input = fs.readFileSync(0, 'utf8');
  const sep = input.indexOf('---PAGE CONTEXT---');
  let prompt = input;
  let context = '';
  if (sep >= 0) {
    prompt = input.slice(0, sep).trim();
    context = input.slice(sep + '---PAGE CONTEXT---'.length).trim();
  }

  const messages = [
    { role: 'system', content: 'You are the WED browser assistant. Answer concisely and helpfully. If page context is provided, use it; otherwise answer generally.' },
    { role: 'user', content: context && context !== '(none)' ? prompt + '\n\nPage context:\n' + context.slice(0, 6000) : prompt }
  ];

  const ZAI = require('z-ai-web-dev-sdk').default || require('z-ai-web-dev-sdk');
  const zai = await ZAI.create();
  const res = await zai.chat.completions.create({
    model,
    messages,
  });
  const text = res && res.choices && res.choices[0] && res.choices[0].message
    ? res.choices[0].message.content : '(empty response)';
  process.stdout.write(String(text).trim() + '\n');
}

main().catch((e) => {
  process.stderr.write('assistant error: ' + (e && e.message ? e.message : e) + '\n');
  process.exit(1);
});
