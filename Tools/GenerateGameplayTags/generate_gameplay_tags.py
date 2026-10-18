
# -*- coding: utf-8 -*-
"""
GameplayTag 代码生成脚本（支持父 Tag 自动补全）
用法: python generate_gameplay_tags.py <GameplayTags.csv> <OutputDir> [ApiMacro]
"""
import csv
import os
import sys
from pathlib import Path
from collections import defaultdict, OrderedDict
from datetime import datetime

HEADER_WARNING = f"""// =====================================================
// 本文件由脚本自动生成，请勿手动修改！
// 来自脚本 Tools/GenerateGameplayTags/generate_gameplay_tags.py
// 生成时间: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}
// 源表: GameplayTags.csv
// =====================================================
"""

AUTO_PARENT_COMMENT = "Auto-generated parent tag"

# -------------------------------------------------------------------
# 工具：由 TagName 自动生成 C++ 变量名
# -------------------------------------------------------------------
def tag_to_cpp(tag_name):
    return tag_name.replace('.', '_')

# -------------------------------------------------------------------
# 1. 读表
# -------------------------------------------------------------------
def _read_csv_text(csv_path):
    """按优先级尝试多种编码读取 CSV 文件"""
    encodings = ['utf-8-sig', 'utf-8', 'gbk', 'gb18030']
    last_err = None
    for enc in encodings:
        try:
            with open(csv_path, 'r', encoding=enc, newline='') as f:
                text = f.read()
            print(f'[INFO] CSV 使用 {enc} 编码读取成功')
            return text
        except UnicodeDecodeError as e:
            last_err = e
            continue
    raise RuntimeError(f'无法识别 CSV 编码，最后错误: {last_err}')


def load_tags(csv_path):
    import io
    tags = OrderedDict()
    text = _read_csv_text(csv_path)
    reader = csv.DictReader(io.StringIO(text))
    for row in reader:
        tag_name = (row.get('TagName') or '').strip()
        if not tag_name:
            continue
        if not _is_valid_tag(tag_name):
            print(f'[ERROR] 非法 Tag 名: "{tag_name}"，跳过')
            continue
        if tag_name in tags:
            print(f'[WARN] 重复 Tag: {tag_name}，后者覆盖前者')
        tags[tag_name] = {
            'TagName'    : tag_name,
            'CppName'    : tag_to_cpp(tag_name),
            'Category'   : (row.get('Category') or 'Common').strip(),
            'Comment'    : (row.get('Comment') or '').strip(),
            'AutoParent' : False,
        }
    return tags

def _is_valid_tag(name):
    if name.startswith('.') or name.endswith('.'):
        return False
    if '..' in name:
        return False
    for seg in name.split('.'):
        if not seg:
            return False
        # C++ 标识符校验：首字符必须是字母或下划线，其余为字母/数字/下划线
        if not (seg[0].isalpha() or seg[0] == '_'):
            return False
        if not all(c.isalnum() or c == '_' for c in seg):
            return False
    return True

# -------------------------------------------------------------------
# 2. 父 Tag 自动补全
# -------------------------------------------------------------------
def expand_parents(tags):
    original_names = list(tags.keys())

    parent_to_children = defaultdict(list)
    for name in original_names:
        parts = name.split('.')
        for i in range(1, len(parts)):
            parent = '.'.join(parts[:i])
            parent_to_children[parent].append(name)

    for parent, children in parent_to_children.items():
        if parent in tags:
            continue
        inherit_category = tags[children[0]]['Category']
        tags[parent] = {
            'TagName'    : parent,
            'CppName'    : tag_to_cpp(parent),
            'Category'   : inherit_category,
            'Comment'    : AUTO_PARENT_COMMENT,
            'AutoParent' : True,
        }

    return OrderedDict(sorted(tags.items(), key=lambda kv: kv[0]))

# -------------------------------------------------------------------
# 3. 校验
# -------------------------------------------------------------------
def validate(tags):
    # 因为 CppName 由 TagName 一对一映射，并且 TagName 本身唯一，
    # 所以 CppName 必然唯一，这里只保留一个兜底校验。
    cpp_names = {}
    ok = True
    for t in tags.values():
        if t['CppName'] in cpp_names:
            print(f'[ERROR] CppName 冲突: {t["CppName"]} 同时用于 '
                  f'"{cpp_names[t["CppName"]]}" 和 "{t["TagName"]}"')
            ok = False
        else:
            cpp_names[t['CppName']] = t['TagName']
    return ok

# -------------------------------------------------------------------
# 4. 生成头文件
# -------------------------------------------------------------------
def gen_header(tags, out_path, api_macro):
    grouped = defaultdict(list)
    for t in tags.values():
        grouped[t['Category']].append(t)

    lines = [HEADER_WARNING, '#pragma once', '',
             '#include "CoreMinimal.h"', '',
             '#include "NativeGameplayTags.h"', '',
             'namespace BrotatoTags', '{']
    for category in sorted(grouped.keys()):
        items = sorted(grouped[category], key=lambda x: x['TagName'])
        lines.append(f'    // =============== {category} ===============')
        for t in items:
            comment = t['Comment'] or t['TagName']
            if t['AutoParent']:
                comment = f'[Auto] {comment}'
            lines.append(f'    // {comment}')
            api_prefix = f'{api_macro} ' if api_macro else ''
            lines.append(f'    {api_prefix}UE_DECLARE_GAMEPLAY_TAG_EXTERN({t["CppName"]});')
        lines.append('')
    lines.append('}')
    _write_if_changed(out_path, '\n'.join(lines))

# -------------------------------------------------------------------
# 5. 生成源文件
# -------------------------------------------------------------------
def gen_source(tags, out_path, header_include):
    lines = [HEADER_WARNING,
             f'#include "{header_include}"', '',
             'namespace BrotatoTags', '{']
    for t in tags.values():
        comment = (t['Comment'] or t['TagName']).replace('"', '\\"')
        if t['AutoParent']:
            comment = f'[Auto] {comment}'
        lines.append(
            f'    UE_DEFINE_GAMEPLAY_TAG_COMMENT({t["CppName"]}, '
            f'"{t["TagName"]}", "{comment}");'
        )
    lines.append('}')
    _write_if_changed(out_path, '\n'.join(lines))

# -------------------------------------------------------------------
# 6. 生成 ini
# -------------------------------------------------------------------
def gen_ini(tags, out_path):
    lines = ['; 自动生成，请勿手改',
             '[/Script/GameplayTags.GameplayTagsSettings]']
    for t in tags.values():
        comment = t['Comment'] or t['TagName']
        if t['AutoParent']:
            comment = f'[Auto] {comment}'
        lines.append(
            f'+GameplayTagList=(Tag="{t["TagName"]}",DevComment="{comment}")'
        )
    _write_if_changed(out_path, '\n'.join(lines))

# -------------------------------------------------------------------
# 7. 工具：内容无变化不改写文件
# -------------------------------------------------------------------
def _write_if_changed(path, content):
    if os.path.exists(path):
        with open(path, 'r', encoding='utf-8') as f:
            if f.read() == content:
                print(f'[SKIP] {path} 无变化')
                return
    with open(path, 'w', encoding='utf-8') as f:
        f.write(content)
    print(f'[WRITE] {path}')

# -------------------------------------------------------------------
# main
# -------------------------------------------------------------------
def main():

    # csv_path  = sys.argv[1]
    # out_dir   = sys.argv[2]
    # api_macro = sys.argv[3] if len(sys.argv) > 3 else ''


    # 拼接存放tag的源代码目录
    script_dir = Path(__file__).resolve().parent
    tagCsvFile = script_dir.parent.parent / "Config/tags.csv"
    tagHeaderDir = script_dir.parent.parent / "Source/BrotatoWithUE5/Public/BrotatoGAS/Tags"
    tagSourcesDir = script_dir.parent.parent / "Source/BrotatoWithUE5/Private/BrotatoGAS/Tags"


    os.makedirs(tagHeaderDir, exist_ok=True)
    os.makedirs(tagSourcesDir, exist_ok=True)

    tags = load_tags(tagCsvFile)
    before = len(tags)
    tags = expand_parents(tags)
    after = len(tags)
    print(f'[INFO] 显式 Tag {before} 个，自动补全父 Tag {after - before} 个，合计 {after} 个')

    if not validate(tags):
        print('[FATAL] 校验失败，终止')
        sys.exit(2)



    gen_header(tags, os.path.join(tagHeaderDir, 'BrotatoNativeTags.h'), '')
    gen_source(tags, os.path.join(tagSourcesDir, 'BrotatoNativeTags.cpp'), 'BrotatoGAS/Tags/BrotatoNativeTags.h')
    # gen_ini   (tags, os.path.join(out_dir, 'DefaultGameplayTags.ini'))
    print('[OK] 生成完成')

if __name__ == '__main__':
    main()

