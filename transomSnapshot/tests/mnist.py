import argparse
import time

import torch
import torch.nn as nn
import torch.nn.functional as F
import torch.optim as optim
from engine import engine
from torch.optim.lr_scheduler import StepLR
from torchvision import datasets, transforms


class Net(nn.Module):
    def __init__(self):
        super(Net, self).__init__()
        self.conv1 = nn.Conv2d(1, 32, 3, 1)
        self.conv2 = nn.Conv2d(32, 64, 3, 1)
        self.dropout1 = nn.Dropout(0.25)
        self.dropout2 = nn.Dropout(0.5)
        self.fc1 = nn.Linear(9216, 128)
        self.fc2 = nn.Linear(128, 10)

    def forward(self, x):
        x = self.conv1(x)
        x = F.relu(x)
        x = self.conv2(x)
        x = F.relu(x)
        x = F.max_pool2d(x, 2)
        x = self.dropout1(x)
        x = torch.flatten(x, 1)
        x = self.fc1(x)
        x = F.relu(x)
        x = self.dropout2(x)
        x = self.fc2(x)
        output = F.log_softmax(x, dim=1)
        return output


def train(args, model, device, train_loader, optimizer, epoch):
    model.train()
    for batch_idx, (data, target) in enumerate(train_loader):
        data, target = data.to(device), target.to(device)
        optimizer.zero_grad()
        output = model(data)
        loss = F.nll_loss(output, target)
        loss.backward()
        optimizer.step()
        if batch_idx % args.log_interval == 0:
            print(
                "Train Epoch: {} [{}/{} ({:.0f}%)]\tLoss: {:.6f}".format(
                    epoch,
                    batch_idx * len(data),
                    len(train_loader.dataset),
                    100.0 * batch_idx / len(train_loader),
                    loss.item(),
                )
            )
            if args.dry_run:
                break


def test(model, device, test_loader):
    model.eval()
    test_loss = 0
    correct = 0
    with torch.no_grad():
        for data, target in test_loader:
            data, target = data.to(device), target.to(device)
            output = model(data)
            # sum up batch loss
            test_loss += F.nll_loss(output, target, reduction="sum").item()
            # get the index of the max log-probability
            pred = output.argmax(dim=1, keepdim=True)
            correct += pred.eq(target.view_as(pred)).sum().item()

    test_loss /= len(test_loader.dataset)

    print(
        "\nTest set: Average loss: {:.4f}, Accuracy: {}/{} ({:.0f}%)\n".format(
            test_loss,
            correct,
            len(test_loader.dataset),
            100.0 * correct / len(test_loader.dataset),
        )
    )


def main():
    # Training settings
    parser = argparse.ArgumentParser(description="PyTorch MNIST Example")
    parser.add_argument(
        "--batch-size",
        type=int,
        default=64,
        metavar="N",
        help="input batch size for training (default: 64)",
    )
    parser.add_argument(
        "--test-batch-size",
        type=int,
        default=1000,
        metavar="N",
        help="input batch size for testing (default: 1000)",
    )
    parser.add_argument(
        "--epochs",
        type=int,
        default=1,
        metavar="N",
        help="number of epochs to train (default: 14)",
    )
    parser.add_argument(
        "--lr",
        type=float,
        default=1.0,
        metavar="LR",
        help="learning rate (default: 1.0)",
    )
    parser.add_argument(
        "--gamma",
        type=float,
        default=0.7,
        metavar="M",
        help="Learning rate step gamma (default: 0.7)",
    )
    parser.add_argument(
        "--no-cuda", action="store_true", default=False, help="disables CUDA training"
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        default=False,
        help="quickly check a single pass",
    )
    parser.add_argument(
        "--seed", type=int, default=1, metavar="S", help="random seed (default: 1)"
    )
    parser.add_argument(
        "--log-interval",
        type=int,
        default=10,
        metavar="N",
        help="how many batches to wait before logging training status",
    )
    parser.add_argument(
        "--save-model",
        action="store_true",
        default=False,
        help="For Saving the current Model",
    )
    args = parser.parse_args()
    use_cuda = not args.no_cuda and torch.cuda.is_available()
    torch.manual_seed(args.seed)

    if use_cuda:
        device = torch.device("cuda")
    else:
        device = torch.device("cpu")

    train_kwargs = {"batch_size": args.batch_size}
    test_kwargs = {"batch_size": args.test_batch_size}
    if use_cuda:
        cuda_kwargs = {"num_workers": 1, "pin_memory": True, "shuffle": True}
        train_kwargs.update(cuda_kwargs)
        test_kwargs.update(cuda_kwargs)

    transform = transforms.Compose(
        [transforms.ToTensor(), transforms.Normalize((0.1307,), (0.3081,))]
    )
    # dataset1 = datasets.MNIST(
    #     "/home/xialei1/device-proxy/examples/pytorch/examples/data",
    #     train=True,
    #     download=True,
    #     transform=transform,
    # )
    # dataset2 = datasets.MNIST(
    #     "/home/xialei1/device-proxy/examples/pytorch/examples/data",
    #     train=False,
    #     transform=transform,
    # )
    # train_loader = torch.utils.data.DataLoader(dataset1, **train_kwargs)
    # test_loader = torch.utils.data.DataLoader(dataset2, **test_kwargs)

    model = Net().to(device)
    optimizer = optim.Adadelta(model.parameters(), lr=args.lr)

    scheduler = StepLR(optimizer, step_size=1, gamma=args.gamma)
    # construct a tensor to expand checkpoint
    start = time.perf_counter()
    # raw_data = torch.randn(33554432, dtype=torch.float32)  # 125MB
    raw_data = torch.randn(33554432, dtype=torch.float32).cuda()  # 125MB
    data = raw_data.repeat(20)
    data1 = raw_data.repeat(20)
    data2 = raw_data.repeat(20)
    data3 = raw_data.repeat(20)
    # data = [raw_data.repeat(10),raw_data.repeat(10),raw_data.repeat(10),raw_data.repeat(10)]
    # data1 = [raw_data.repeat(10),raw_data.repeat(10),raw_data.repeat(10),raw_data.repeat(10)]
    # data2 = [raw_data.repeat(10),raw_data.repeat(10),raw_data.repeat(10),raw_data.repeat(10)]
    # data3 = [raw_data.repeat(10),raw_data.repeat(10),raw_data.repeat(10),raw_data.repeat(10)]
    elapsed = time.perf_counter() - start
    print(f"save data: {data[:30]}")
    print(f"generate data in {elapsed:0.4f} seconds.")

    total_start = time.perf_counter()
    for epoch in range(1, args.epochs + 1):
        # s = time.perf_counter()
        # # train
        # train(args, model, device, train_loader, optimizer, epoch)
        # elapsed = time.perf_counter() - s
        # print(f"{epoch} train in {elapsed:0.4f} seconds.")

        # save checkpoint
        model_state_dict = model.state_dict()
        optimizer_state_dict = optimizer.state_dict()
        start = time.perf_counter()
        engine.save(
            {
                # "epoch": epoch,
                # "model_state_dict": model_state_dict,
                # "optimizer_state_dict": optimizer_state_dict,
                "data": data,
                "data1": data1,
                "data2": data2,
                "data3": data3,
            },
            "/tmp/ckpt-{}.pt".format(epoch),
            _use_new_zipfile_serialization=False,
        )
        # engine.save(
        #     {
        #         "data": data1,
        #     },
        #     "/tmp/ckpt-{}.pt".format(epoch+1),
        #     _use_new_zipfile_serialization=False,
        # )

        # engine.save(
        #     {
        #         "data": data2,
        #     },
        #     "/tmp/ckpt-{}.pt".format(epoch+2),
        #     _use_new_zipfile_serialization=False,
        # )

        # engine.save(
        #     {
        #         "data": data3,
        #     },
        #     "/tmp/ckpt-{}.pt".format(epoch+3),
        #     _use_new_zipfile_serialization=False,
        # )
        # engine.wait()
        elapsed = time.perf_counter() - start
        print(f"/tmp/ckpt-{epoch}.pt save in {elapsed:0.4f} seconds.")
        # simulation training process
        # time.sleep(3)
        # load checkpoint
        start = time.perf_counter()
        load_checkpoint = engine.load("/tmp/ckpt-{}.pt".format(epoch))
        elapsed = time.perf_counter() - start
        print(f"/tmp/ckpt-{epoch}.pt load in {elapsed:0.4f} seconds.")

        # validate load
        # load_model_state_dict = load_checkpoint["model_state_dict"]
        # load_optimizer_state_dict = load_checkpoint["optimizer_state_dict"]
        # assert str(model_state_dict) == str(load_model_state_dict)
        # assert str(optimizer_state_dict) == str(load_optimizer_state_dict)

        load_data = load_checkpoint["data"]
        print(f"load data: {load_data[:30]}")
        print(
            "save_data == load_data?",
            torch.eq(data, load_data),
            torch.equal(data, load_checkpoint["data"]),
            torch.equal(data1, load_checkpoint["data1"]),
            torch.equal(data2, load_checkpoint["data2"]),
            torch.equal(data3, load_checkpoint["data3"]),
        )
        # return
        # test(model, device, test_loader)
        # scheduler.step()
    elapsed = time.perf_counter() - total_start
    print(f"total {elapsed:0.4f} seconds.")

    if args.save_model:
        engine.save(model.state_dict(), "mnist_cnn.pt")


if __name__ == "__main__":
    main()
