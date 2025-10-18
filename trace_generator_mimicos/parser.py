import sys
def extract_first_numbers(input_file):
    first_numbers = []
    
    try:
        with open(input_file, 'r') as infile:
            for line in infile:
                # Split the line into two parts
                parts = line.split()
                if len(parts) >= 2:  # Ensure there's at least one number
                    first_numbers.append([float(parts[0]), float(parts[2])])  # Convert to float or int as needed
        #print(f"Extracted first numbers: {first_numbers}")
    except Exception as e:
        print(f"An error occurred: {e}")
    return first_numbers

def read_and_write(input_file,input_file2, output_file, pagefaults_log_file):
    line_numbers = extract_first_numbers(pagefaults_log_file)

    line_number=0
    line_entry_index=0
    try:
        with open(input_file, 'r') as infile,open(input_file2, 'r') as infile2, open(output_file, 'w') as outfile:
            line_count = 0
            for line in infile:
                # parts= line.split()
                # if len(parts) >= 2:  # Ensure there's at least one number
                #     line_number=int(parts[1])  # Convert to float or int as needed
                # else:
                #     continue
                is_pf=False
                if(line_entry_index<len(line_numbers)):
                    if(line_count==int(line_numbers[line_entry_index][0])):
                        #print(line_number,int(line_numbers[line_entry_index]))
                        is_pf=True
                        while(1):
                            additional_line = infile2.readline()
                            if additional_line:  # Check if the line is not empty
                                if(additional_line[0]=='-'):
                                    break
                                else:
                                    outfile.write(additional_line)
                            else:
                                break  # Stop if there are no more lines in the second file
                        line_entry_index+=1	
                line_count += 1
                # if extra=="":
                #     outfile.write(line)
                # else:
                #     outfile.write(line)
                if not is_pf:
                    outfile.write(line)
                # else:
                #     print(line.split()[0]+ " "+ str(int(line_numbers[line_entry_index-1][1])))
                #     outfile.write(line.split()[0]+ " "+ str(int(line_numbers[line_entry_index-1][1]))+ "\n")

        print(f"Successfully copied content from {input_file} to {output_file}.")
    except Exception as e:
        print(f"An error occurred: {e}")

if __name__ == "__main__":
    if len(sys.argv) != 5:
        print(f"Usage: {sys.argv[0]} <input_file1> <input_file2> <output_file> <pagefaults_log_file>")
        sys.exit(1)

    input_file = sys.argv[1]
    input_file2 = sys.argv[2]
    output_file = sys.argv[3]
    pagefaults_log_file = sys.argv[4]
    read_and_write(input_file, input_file2, output_file, pagefaults_log_file)