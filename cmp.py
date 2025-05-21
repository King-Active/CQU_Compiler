import sys
from collections import defaultdict

def parse_token_file(file_path):
    """解析Token文件，返回(Token类型, Token值)列表"""
    tokens = []
    with open(file_path, 'r') as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            parts = line.split()
            if len(parts) >= 2:
                tokens.append((parts[0], ' '.join(parts[1:])))
    return tokens

def compare_files(file1, file2):
    """比较两个Token文件并生成差异报告"""
    tokens1 = parse_token_file(file1)
    tokens2 = parse_token_file(file2)
    
    # 统计差异
    diff_report = {
        'unique_to_file1': [],
        'unique_to_file2': [],
        'type_differences': [],
        'position_differences': defaultdict(list)
    }
    
    # 检查长度差异
    len_diff = len(tokens1) - len(tokens2)
    if len_diff != 0:
        diff_report['length_difference'] = f"文件1多{len_diff}个Token" if len_diff > 0 else f"文件2多{-len_diff}个Token"
    
    # 逐Token比较
    max_len = max(len(tokens1), len(tokens2))
    for i in range(max_len):
        if i >= len(tokens1):
            diff_report['unique_to_file2'].append((i, tokens2[i]))
            continue
        if i >= len(tokens2):
            diff_report['unique_to_file1'].append((i, tokens1[i]))
            continue
            
        tok1_type, tok1_val = tokens1[i]
        tok2_type, tok2_val = tokens2[i]
        
        if tok1_type != tok2_type:
            diff_report['type_differences'].append(
                (i+1, tok1_type, tok2_type)
            )
        elif tok1_val != tok2_val:
            diff_report['position_differences'][i+1].append(
                (tok1_val, tok2_val)
            )
    
    return tokens1, tokens2, diff_report

def generate_report(file1, file2, tokens1, tokens2, diff_report):
    """生成可读的差异报告"""
    report = []
    report.append("\n===== Token序列对比报告 =====")
    report.append(f"文件1: {file1} (共{len(tokens1)}个Token)")
    report.append(f"文件2: {file2} (共{len(tokens2)}个Token)\n")
    
    if 'length_difference' in diff_report:
        report.append(f"⚠️ Token数量差异: {diff_report['length_difference']}")
    
    if diff_report['unique_to_file1']:
        report.append("\n🔍 文件1独有的Token:")
        for pos, token in diff_report['unique_to_file1']:
            report.append(f"  第{pos+1}行: {token[0]} {token[1]}")
    
    if diff_report['unique_to_file2']:
        report.append("\n🔍 文件2独有的Token:")
        for pos, token in diff_report['unique_to_file2']:
            report.append(f"  第{pos+1}行: {token[0]} {token[1]}")
    
    if diff_report['type_differences']:
        report.append("\n❌ Token类型不匹配:")
        for pos, type1, type2 in diff_report['type_differences']:
            report.append(f"  第{pos}行: 文件1={type1} ↔ 文件2={type2}")
    
    if diff_report['position_differences']:
        report.append("\n🔠 相同类型但值不同:")
        for pos, values in diff_report['position_differences'].items():
            for val1, val2 in values:
                report.append(f"  第{pos}行: 文件1='{val1}' ↔ 文件2='{val2}'")
    
    return '\n'.join(report)

if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("用法: python compare_tokens.py 文件1 文件2")
        sys.exit(1)
    
    file1 = sys.argv[1]
    file2 = sys.argv[2]
    
    tokens1, tokens2, diff_report = compare_files(file1, file2)
    print(generate_report(file1, file2, tokens1, tokens2, diff_report))