import os
from math import isnan
import matplotlib.pyplot as plt
import ast
import pandas as pd
from automatic_annotations import compare_by_slice


def compare_doctors(lesion_dir, doctors_lesion_dirs):
    similarity = [{} for _ in range(len(doctors_lesion_dirs))]

    for filename in os.listdir(lesion_dir):
        if filename.endswith(".raw"):
            _, case_no, img_size, _, slices_no, _, _ = filename.split('_')

            lesions_filepath = lesion_dir + '/' + filename

            for doct_no, direct in enumerate(doctors_lesion_dirs):
                doct_les_filepath = direct + '/' + f'Lesions_{case_no}_{img_size}_{img_size}_{slices_no}_1_.raw'
                similarity[doct_no][case_no] = \
                    compare_by_slice(lesions_filepath, doct_les_filepath, int(img_size), int(slices_no), 255, 1)
        else:
            continue

    for doct_no, direct in enumerate(doctors_lesion_dirs):
        similarity[doct_no] = {k: v for k, v in similarity[doct_no].items() if not isnan(v[0])}

        f = open(direct + '/similarity_by_slice.txt', "w")
        f.write(str(similarity[doct_no]))
        f.close()


def compare_results(original_dir, merged1_dir, merged2_dir, output_paths):
    differences = {}
    similarity_iou = {}
    similarity_dice = {}
    for filename in os.listdir(merged2_dir):
        if filename.endswith(".raw"):
            _, case_no, img_size, _, slices_no, _, _ = filename.split('_')

            lesions_filepath = original_dir + '/' + f'MV_{case_no}_{img_size}_{img_size}_{slices_no}_1_.raw'
            merged1_filepath = merged1_dir + '/' + filename
            merged2_filepath = merged2_dir + '/' + filename

            iou_1, dice_1 = compare_by_slice(lesions_filepath, merged1_filepath, int(img_size), int(slices_no))
            iou_2, dice_2 = compare_by_slice(lesions_filepath, merged2_filepath, int(img_size), int(slices_no))

            differences[case_no] = (abs(iou_1-iou_2), abs(dice_1-dice_2))
            similarity_iou[case_no] = (iou_1, iou_2)
            similarity_dice[case_no] = (dice_1, dice_2)

        else:
            continue

    differences = {k: v for k, v in differences.items() if not isnan(v[0])}
    with open(output_paths[0], 'w') as f:
        f.write(str(differences))

    similarity_iou = {k: v for k, v in similarity_iou.items() if not isnan(v[0])}
    with open(output_paths[1], 'w') as f:
        f.write(str(similarity_iou))

    similarity_dice = {k: v for k, v in similarity_dice.items() if not isnan(v[0])}
    with open(output_paths[2], 'w') as f:
        f.write(str(similarity_dice))


def plot_similarity(filepath, name):
    f = open(filepath, 'r')
    res = ast.literal_eval(f.read())
    f.close()
    df = pd.DataFrame(res, index=['IoU', 'Dice']).T.sort_index()
    df.plot(kind='bar', title=f"{name}: mean(IoU)={round(df['IoU'].mean(),2)}, mean(Dice)={round(df['Dice'].mean(),2)}")
    plt.grid()
    plt.savefig(f'./plots/{name}_by_slice.png')
    plt.clf()


def plot_superpixels_performance(sp_dir, sp_no_list):
    mean_iou = []
    mean_dice = []
    for sp_no in sp_no_list:
        filepath = f'{sp_dir}/{sp_no}/similarity_by_slice.txt'
        f = open(filepath, 'r')
        res = ast.literal_eval(f.read())
        f.close()
        df = pd.DataFrame(res, index=['IoU', 'Dice']).T
        mean_iou.append(df['IoU'].mean())
        mean_dice.append(df['Dice'].mean())

    plt.plot(sp_no_list, mean_iou, 'o-', label='IoU')
    plt.plot(sp_no_list, mean_dice, 'o-', label='Dice')
    plt.legend()
    plt.grid()
    plt.savefig('./plots/superpixels_performance.png')
    plt.clf()


if __name__ == '__main__':
    ORIGINAL_DIR = '../../../SpA_Data/GroundTruth_MajorityVoting'
    MERGED1_DIR = '../../../SpA_Data/Merged/Lesions(Q25)'
    MERGED2_DIR = '../../../SpA_Data/Merged/Lesions(Q25)-no_filter'
    OUTPUT_PATHS = ['./outputs/differences.txt',
                    './outputs/iou.txt',
                    './outputs/dice.txt']

    compare_results(ORIGINAL_DIR, MERGED1_DIR, MERGED2_DIR, OUTPUT_PATHS)

    DOCT_LES_DIR_LIST = ['../../../SpA_Data/Lesions_IK', '../../../SpA_Data/Lesions_WW', '../../../SpA_Data/Lesions_3rd']
    compare_doctors(ORIGINAL_DIR, DOCT_LES_DIR_LIST)

    SIMILARITY_PATH_LIST = [('../../../SpA_Data/Merged/Lesions(Q25)/similarity_by_slice.txt', 'Q25'),
                            ('../../../SpA_Data/Merged/Lesions(Q10)/similarity_by_slice.txt', 'Q10'),
                            ('../../../SpA_Data/Lesions_IK/similarity_by_slice.txt', 'IK'),
                            ('../../../SpA_Data/Lesions_WW/similarity_by_slice.txt', 'WW'),
                            ('../../../SpA_Data/Lesions_3rd/similarity_by_slice.txt', '3rd'),
                            ]

    for PATH, NAME in SIMILARITY_PATH_LIST:
        plot_similarity(PATH, NAME)

    plot_similarity('./outputs/differences.txt', 'Difference')
